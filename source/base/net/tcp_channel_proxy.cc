//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/net/tcp_channel_proxy.h"

#include <asio/post.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include "base/location.h"
#include "base/net/tcp_channel_native.h"
#include "base/threading/asio_event_dispatcher.h"

namespace {

//--------------------------------------------------------------------------------------------------
// Returns an invalid URL if |host| is not a valid host name or address.
QUrl makeUrl(const QString& scheme, const QString& host, quint16 port)
{
    QUrl url;
    url.setScheme(scheme);
    url.setHost(host);
    url.setPort(port);

    if (!url.isValid() || url.host().isEmpty())
        return QUrl();

    return url;
}

//--------------------------------------------------------------------------------------------------
bool isIpAddress(const QString& host)
{
    std::error_code error_code;
    asio::ip::make_address(host.toStdString(), error_code);
    return !error_code;
}

//--------------------------------------------------------------------------------------------------
// Returns the entry for CURLOPT_RESOLVE, it looks like "host:port:address,[address]".
std::string resolveEntry(const QUrl& url, const asio::ip::tcp::resolver::results_type& endpoints)
{
    std::string entry = url.host().toStdString() + ':' + std::to_string(url.port()) + ':';

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
    {
        const asio::ip::address address = it->endpoint().address();

        if (it != endpoints.begin())
            entry += ',';

        entry += address.is_v6() ? '[' + address.to_string() + ']' : address.to_string();
    }

    return entry;
}

//--------------------------------------------------------------------------------------------------
const char* proxyScheme(ProxySettings::Type type)
{
    switch (type)
    {
        case ProxySettings::Type::HTTP: return "http";
        case ProxySettings::Type::HTTPS: return "https";
        case ProxySettings::Type::SOCKS4: return "socks4";
        case ProxySettings::Type::SOCKS4A: return "socks4a";
        case ProxySettings::Type::SOCKS5: return "socks5";
        case ProxySettings::Type::SOCKS5H: return "socks5h";
    }

    return "http";
}

//--------------------------------------------------------------------------------------------------
// The interest of curl that a wait of |type| serves. An error concerns any socket curl watches.
int pollBits(asio::socket_base::wait_type type)
{
    switch (type)
    {
        case asio::socket_base::wait_read: return CURL_POLL_IN;
        case asio::socket_base::wait_write: return CURL_POLL_OUT;
        default: return CURL_POLL_INOUT;
    }
}

//--------------------------------------------------------------------------------------------------
int selectAction(asio::socket_base::wait_type type)
{
    switch (type)
    {
        case asio::socket_base::wait_read: return CURL_CSELECT_IN;
        case asio::socket_base::wait_write: return CURL_CSELECT_OUT;
        default: return CURL_CSELECT_ERR;
    }
}

//--------------------------------------------------------------------------------------------------
// |proxy_response| is the HTTP code of the CONNECT response, |proxy_error| is the CURLproxycode of a
// SOCKS failure. The addresses were checked before curl ran, so an unrecognized code means that the
// talk with the proxy failed, TLS to an HTTPS proxy included.
TcpChannel::ErrorCode tunnelErrorCode(CURLcode result, long proxy_response, long proxy_error)
{
    if (proxy_response == 407)
        return TcpChannel::ErrorCode::PROXY_AUTH_REQUIRED;

    switch (result)
    {
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
            return TcpChannel::ErrorCode::SPECIFIED_HOST_NOT_FOUND;

        case CURLE_COULDNT_CONNECT:
            // The proxy itself answered with a non-2xx response, this is not a refusal of the TCP connection.
            return proxy_response ? TcpChannel::ErrorCode::PROXY_ERROR : TcpChannel::ErrorCode::CONNECTION_REFUSED;

        case CURLE_OPERATION_TIMEDOUT:
            return TcpChannel::ErrorCode::SOCKET_TIMEOUT;

        case CURLE_PROXY:
            switch (proxy_error)
            {
                case CURLPX_USER_REJECTED: // SOCKS5 rejected the name or the password.
                case CURLPX_NO_AUTH:       // SOCKS5 accepted none of the offered methods.
                case CURLPX_IDENTD:        // SOCKS4 rejected the user by identd.
                case CURLPX_IDENTD_DIFFER:
                    return TcpChannel::ErrorCode::PROXY_AUTH_REQUIRED;
                default:
                    return TcpChannel::ErrorCode::PROXY_ERROR;
            }

        default:
            return TcpChannel::ErrorCode::PROXY_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
std::string endpointToString(const asio::ip::tcp::endpoint& endpoint)
{
    const asio::ip::address address = endpoint.address();
    const std::string host = address.is_v6() ? '[' + address.to_string() + ']' : address.to_string();
    return host + ':' + std::to_string(endpoint.port());
}

//--------------------------------------------------------------------------------------------------
const char* proxyAuthName(long auth)
{
    switch (auth)
    {
        case CURLAUTH_NONE: return "none";
        case CURLAUTH_BASIC: return "Basic";
        case CURLAUTH_DIGEST: return "Digest";
        case CURLAUTH_NTLM: return "NTLM";
        case CURLAUTH_NEGOTIATE: return "Negotiate";
        default: return "unknown";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
TcpChannelProxy::TcpChannelProxy(
    Type type, const ProxySettings& settings, Authenticator* authenticator, QObject* parent)
    : TcpChannel(type, authenticator, parent),
      io_context_(AsioEventDispatcher::ioContext()),
      resolver_(io_context_),
      connect_timer_(io_context_),
      multi_timer_(io_context_),
      settings_(settings)
{
    curl_multi_setopt(multi_.get(), CURLMOPT_SOCKETFUNCTION, socketCallback);
    curl_multi_setopt(multi_.get(), CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(multi_.get(), CURLMOPT_TIMERFUNCTION, timerCallback);
    curl_multi_setopt(multi_.get(), CURLMOPT_TIMERDATA, this);
}

//--------------------------------------------------------------------------------------------------
TcpChannelProxy::~TcpChannelProxy()
{
    closeTransport();
}

//--------------------------------------------------------------------------------------------------
// static
TcpChannel* TcpChannelProxy::create(Type type, const std::optional<ProxySettings>& settings,
                                    Authenticator* authenticator, QObject* parent)
{
    if (!settings.has_value())
        return new TcpChannelNative(type, authenticator, parent);

    return new TcpChannelProxy(type, *settings, authenticator, parent);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::connectTo(const QString& address, quint16 port, Seconds timeout)
{
    if (isConnected() || !ioState()->alive)
        return;

    peer_url_ = makeUrl("http", address, port);
    proxy_url_ = makeUrl(proxyScheme(settings_.type), settings_.address, settings_.port);
    if (!peer_url_.isValid() || !proxy_url_.isValid())
    {
        CLOG(ERROR) << "Invalid address of the peer or of the proxy";
        postError(ErrorCode::SPECIFIED_HOST_NOT_FOUND);
        return;
    }

    peer_address_ = address.toStdString();

    curl_easy_setopt(curl_.get(), CURLOPT_URL, peer_url_.toEncoded().constData());
    curl_easy_setopt(curl_.get(), CURLOPT_PROXY, proxy_url_.toEncoded().constData());
    curl_easy_setopt(curl_.get(), CURLOPT_HTTPPROXYTUNNEL, 1L);
    curl_easy_setopt(curl_.get(), CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(curl_.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_.get(), CURLOPT_OPENSOCKETFUNCTION, openSocketCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_OPENSOCKETDATA, this);
    curl_easy_setopt(curl_.get(), CURLOPT_CLOSESOCKETFUNCTION, closeSocketCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_CLOSESOCKETDATA, this);
    curl_easy_setopt(curl_.get(), CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl_.get(), CURLOPT_DEBUGFUNCTION, debugCallback);
    curl_easy_setopt(curl_.get(), CURLOPT_DEBUGDATA, this);

    if (!settings_.user.isEmpty())
    {
        curl_easy_setopt(curl_.get(), CURLOPT_PROXYAUTH, static_cast<long>(CURLAUTH_ANY));
        curl_easy_setopt(curl_.get(), CURLOPT_PROXYUSERNAME, settings_.user.toUtf8().constData());
        curl_easy_setopt(curl_.get(), CURLOPT_PROXYPASSWORD, settings_.password.toUtf8().constData());
    }

    // Watchdog for the entire connect phase (resolve, connection to the proxy and the tunnel).
    connect_timer_.expires_after(timeout);
    connect_timer_.async_wait([this, io = ioState()](const std::error_code& error_code)
    {
        if (io->alive && !error_code && !isConnected())
            onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    });

    CLOG(INFO) << "Connecting to" << address << ":" << port << "through" << settings_.type << "proxy"
               << settings_.address << ":" << settings_.port;

    // curl resolves names in a thread that wakes the multi through a socket of its own, and nothing
    // here watches that socket. So the names are resolved here and handed to curl in its DNS cache.
    resolveProxy();
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelProxy::setReadBufferSize(int size)
{
    if (!tunnel_)
    {
        CLOG(ERROR) << "Tunnel is not opened";
        return false;
    }

    asio::socket_base::receive_buffer_size option(size);

    asio::error_code error_code;
    tunnel_->socket.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set read buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Read buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelProxy::setWriteBufferSize(int size)
{
    if (!tunnel_)
    {
        CLOG(ERROR) << "Tunnel is not opened";
        return false;
    }

    asio::socket_base::send_buffer_size option(size);

    asio::error_code error_code;
    tunnel_->socket.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Write buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::readTransport(void* data, size_t size)
{
    // TLS to an HTTPS proxy lives inside curl, so the data goes through curl. Other proxies leave a
    // plain tunnel in the socket.
    if (settings_.type == ProxySettings::Type::HTTPS)
    {
        read_data_ = static_cast<char*>(data);
        read_size_ = size;
        read_done_ = 0;

        // The base class expects the completion after this call returns, even when the data is here.
        asio::post(io_context_, [this, io = ioState()]()
        {
            if (io->alive)
                continueRead();
        });
        return;
    }

    asio::async_read(tunnel_->socket, asio::buffer(data, size),
        [this, io = ioState()](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        onReadCompleted(error_code, bytes_transferred);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::writeTransport(const void* data, size_t size)
{
    if (settings_.type == ProxySettings::Type::HTTPS)
    {
        write_data_ = static_cast<const char*>(data);
        write_size_ = size;
        write_done_ = 0;

        asio::post(io_context_, [this, io = ioState()]()
        {
            if (io->alive)
                continueWrite();
        });
        return;
    }

    asio::async_write(tunnel_->socket, asio::buffer(data, size),
        [this, io = ioState()](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        onWriteCompleted(error_code, bytes_transferred);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::closeTransport()
{
    // Handlers that are already queued must not touch the channel once the transport is gone.
    ioState()->alive = false;

    connect_timer_.cancel();
    resolver_.cancel();
    multi_timer_.cancel();

    // curl closes its sockets through closeSocket while the handle is removed. Whatever is left is
    // closed here.
    curl_multi_setopt(multi_.get(), CURLMOPT_SOCKETFUNCTION, nullptr);
    curl_multi_setopt(multi_.get(), CURLMOPT_TIMERFUNCTION, nullptr);
    curl_multi_remove_handle(multi_.get(), curl_.get());

    // The handle is cleaned up after the members it would log through are gone.
    curl_easy_setopt(curl_.get(), CURLOPT_VERBOSE, 0L);

    for (auto& [socket, curl_socket] : sockets_)
    {
        std::error_code ignored_code;
        curl_socket->socket.close(ignored_code);
    }

    sockets_.clear();
}

//--------------------------------------------------------------------------------------------------
// static
curl_socket_t TcpChannelProxy::openSocketCallback(
    void* clientp, curlsocktype /* purpose */, curl_sockaddr* address)
{
    return static_cast<TcpChannelProxy*>(clientp)->openSocket(address->family);
}

//--------------------------------------------------------------------------------------------------
// static
int TcpChannelProxy::closeSocketCallback(void* clientp, curl_socket_t socket)
{
    return static_cast<TcpChannelProxy*>(clientp)->closeSocket(socket);
}

//--------------------------------------------------------------------------------------------------
// static
int TcpChannelProxy::socketCallback(
    CURL* /* easy */, curl_socket_t socket, int what, void* userp, void* /* socketp */)
{
    static_cast<TcpChannelProxy*>(userp)->watchSocket(socket, what);
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
int TcpChannelProxy::timerCallback(CURLM* /* multi */, long timeout_ms, void* userp)
{
    static_cast<TcpChannelProxy*>(userp)->setMultiTimeout(timeout_ms);
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
int TcpChannelProxy::debugCallback(CURL* /* easy */, curl_infotype type, char* data, size_t size, void* clientp)
{
    static_cast<TcpChannelProxy*>(clientp)->onDebugMessage(type, std::string_view(data, size));
    return 0;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::resolveProxy()
{
    if (isIpAddress(proxy_url_.host()))
    {
        resolvePeer();
        return;
    }

    resolver_.async_resolve(proxy_url_.host().toStdString(), std::to_string(proxy_url_.port()),
        [this, io = ioState()](const std::error_code& error_code,
                               const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (io->alive && onResolved(proxy_url_, error_code, endpoints))
            resolvePeer();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::resolvePeer()
{
    // SOCKS4 and SOCKS5 send an address to the proxy, so curl resolves the name of the peer itself.
    const bool local = settings_.type == ProxySettings::Type::SOCKS4 || settings_.type == ProxySettings::Type::SOCKS5;
    if (!local || isIpAddress(peer_url_.host()))
    {
        startTransfer();
        return;
    }

    resolver_.async_resolve(peer_url_.host().toStdString(), std::to_string(peer_url_.port()),
        [this, io = ioState()](const std::error_code& error_code,
                               const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (io->alive && onResolved(peer_url_, error_code, endpoints))
            startTransfer();
    });
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelProxy::onResolved(const QUrl& url, const std::error_code& error_code,
                                 const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (error_code)
    {
        if (error_code != asio::error::operation_aborted)
            onErrorOccurred(FROM_HERE, error_code);
        return false;
    }

    const std::string entry = resolveEntry(url, endpoints);
    CLOG(INFO) << "Resolved:" << entry;

    if (!resolve_list_.append(entry.c_str()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::startTransfer()
{
    curl_easy_setopt(curl_.get(), CURLOPT_RESOLVE, resolve_list_.get());

    CURLMcode error_code = curl_multi_add_handle(multi_.get(), curl_.get());
    if (error_code != CURLM_OK)
    {
        CLOG(ERROR) << "curl_multi_add_handle failed:" << curl_multi_strerror(error_code);
        postError(ErrorCode::UNKNOWN);
    }
}

//--------------------------------------------------------------------------------------------------
curl_socket_t TcpChannelProxy::openSocket(int family)
{
    SharedPointer<CurlSocket> curl_socket(new CurlSocket(io_context_));

    std::error_code error_code;
    curl_socket->socket.open(family == AF_INET6 ? asio::ip::tcp::v6() : asio::ip::tcp::v4(), error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Unable to open socket:" << error_code;
        return CURL_SOCKET_BAD;
    }

    curl_socket_t socket = curl_socket->socket.native_handle();
    sockets_[socket] = curl_socket;
    return socket;
}

//--------------------------------------------------------------------------------------------------
int TcpChannelProxy::closeSocket(curl_socket_t socket)
{
    auto it = sockets_.find(socket);
    if (it == sockets_.end())
        return 0;

    std::error_code ignored_code;
    it->second->socket.close(ignored_code);
    sockets_.erase(it);
    return 0;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::watchSocket(curl_socket_t socket, int what)
{
    // Once the tunnel is open the socket belongs to the transport.
    if (tunnel_)
        return;

    auto it = sockets_.find(socket);
    if (it == sockets_.end())
        return;

    // CURL_POLL_REMOVE has none of the direction bits.
    it->second->what = what & CURL_POLL_INOUT;

    if (what & CURL_POLL_IN)
        waitSocket(it->second, asio::socket_base::wait_read);

    if (what & CURL_POLL_OUT)
        waitSocket(it->second, asio::socket_base::wait_write);

    // Windows reports a failed connect only through the error set of select, a wait for writing never
    // wakes up then. On other systems the error wakes up every wait, the extra one does no harm.
    if (it->second->what)
        waitSocket(it->second, asio::socket_base::wait_error);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::waitSocket(
    const SharedPointer<CurlSocket>& curl_socket, asio::socket_base::wait_type type)
{
    if (curl_socket->waiting(type))
        return;

    curl_socket->waiting(type) = true;

    curl_socket->socket.async_wait(type,
        [this, io = ioState(), curl_socket, type](const std::error_code& error_code)
    {
        curl_socket->waiting(type) = false;

        if (!io->alive || error_code == asio::error::operation_aborted || !curl_socket->socket.is_open())
            return;

        if (!(curl_socket->what & pollBits(type)))
            return;

        // A failure of the wait itself goes to curl as an error of the socket. curl reads the state of the
        // socket and finishes the transfer with a proper code, so the wait is not armed again after it.
        driveMulti(curl_socket->socket.native_handle(), error_code ? CURL_CSELECT_ERR : selectAction(type));

        // curl reports only changes of its interest, so the wait is armed again while it lasts.
        if (!error_code && io->alive && curl_socket->socket.is_open() && (curl_socket->what & pollBits(type)))
            waitSocket(curl_socket, type);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::setMultiTimeout(long timeout_ms)
{
    if (timeout_ms < 0)
    {
        multi_timer_.cancel();
        return;
    }

    multi_timer_.expires_after(MilliSeconds(timeout_ms));
    multi_timer_.async_wait([this, io = ioState()](const std::error_code& error_code)
    {
        if (io->alive && !error_code)
            driveMulti(CURL_SOCKET_TIMEOUT, 0);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::driveMulti(curl_socket_t socket, int action)
{
    if (tunnel_)
        return;

    int running = 0;
    CURLMcode error_code = curl_multi_socket_action(multi_.get(), socket, action, &running);
    if (error_code != CURLM_OK)
    {
        CLOG(ERROR) << "curl_multi_socket_action failed:" << curl_multi_strerror(error_code);
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return;
    }

    int messages_left = 0;
    while (CURLMsg* message = curl_multi_info_read(multi_.get(), &messages_left))
    {
        if (message->msg == CURLMSG_DONE)
        {
            onConnectDone(message->data.result);
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::onConnectDone(CURLcode result)
{
    connect_timer_.cancel();
    multi_timer_.cancel();

    if (result != CURLE_OK)
    {
        long proxy_response = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_HTTP_CONNECTCODE, &proxy_response);

        long proxy_error = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_PROXY_ERROR, &proxy_error);

        CLOG(ERROR) << "Unable to open the tunnel:" << curl_easy_strerror(result)
                    << "(proxy response" << proxy_response << "proxy error" << proxy_error << ")";
        onErrorOccurred(FROM_HERE, tunnelErrorCode(result, proxy_response, proxy_error));
        return;
    }

    curl_socket_t socket = CURL_SOCKET_BAD;
    curl_easy_getinfo(curl_.get(), CURLINFO_ACTIVESOCKET, &socket);

    auto it = sockets_.find(socket);
    if (it == sockets_.end())
    {
        CLOG(ERROR) << "Tunnel is opened on an unknown socket";
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return;
    }

    tunnel_ = it->second;
    tunnel_->what = 0;

    asio::error_code error_code;
    tunnel_->socket.cancel(error_code);

    asio::ip::tcp::no_delay no_delay_option(true);
    tunnel_->socket.set_option(no_delay_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Nagle's algorithm is disabled";

    asio::socket_base::receive_buffer_size receive_option;
    tunnel_->socket.get_option(receive_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Read buffer size:" << receive_option.value();

    asio::socket_base::send_buffer_size send_option;
    tunnel_->socket.get_option(send_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Write buffer size:" << send_option.value();

    // The socket the data goes through, so its peer is the proxy.
    const std::string proxy_endpoint = endpointToString(tunnel_->socket.remote_endpoint(error_code));
    const std::string local_endpoint = endpointToString(tunnel_->socket.local_endpoint(error_code));

    if (settings_.type == ProxySettings::Type::HTTP || settings_.type == ProxySettings::Type::HTTPS)
    {
        long proxy_response = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_HTTP_CONNECTCODE, &proxy_response);

        long proxy_auth = 0;
        curl_easy_getinfo(curl_.get(), CURLINFO_PROXYAUTH_USED, &proxy_auth);

        CLOG(INFO) << "Tunnel to" << peer_address_ << ":" << peer_url_.port() << "is opened through"
                   << proxy_endpoint << "from" << local_endpoint << "(proxy response" << proxy_response
                   << "auth" << proxyAuthName(proxy_auth) << ")";
    }
    else
    {
        CLOG(INFO) << "Tunnel to" << peer_address_ << ":" << peer_url_.port() << "is opened through"
                   << proxy_endpoint << "from" << local_endpoint;
    }

    setConnected(peer_address_);
    emit sig_connected();

    doAuthentication();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::continueRead()
{
    while (read_done_ < read_size_)
    {
        size_t received = 0;
        CURLcode result = curl_easy_recv(
            curl_.get(), read_data_ + read_done_, read_size_ - read_done_, &received);
        if (result == CURLE_AGAIN)
        {
            tunnel_->socket.async_wait(asio::socket_base::wait_read,
                [this, io = ioState()](const std::error_code& error_code)
            {
                if (!io->alive)
                    return;

                if (error_code)
                {
                    onReadCompleted(error_code, 0);
                    return;
                }

                continueRead();
            });
            return;
        }

        if (result != CURLE_OK)
        {
            onTransportError(result);
            return;
        }

        if (!received)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::REMOTE_HOST_CLOSED);
            return;
        }

        read_done_ += received;
    }

    onReadCompleted(std::error_code(), read_size_);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::continueWrite()
{
    while (write_done_ < write_size_)
    {
        size_t sent = 0;
        CURLcode result = curl_easy_send(
            curl_.get(), write_data_ + write_done_, write_size_ - write_done_, &sent);
        if (result == CURLE_AGAIN)
        {
            tunnel_->socket.async_wait(asio::socket_base::wait_write,
                [this, io = ioState()](const std::error_code& error_code)
            {
                if (!io->alive)
                    return;

                if (error_code)
                {
                    onWriteCompleted(error_code, 0);
                    return;
                }

                continueWrite();
            });
            return;
        }

        if (result != CURLE_OK)
        {
            onTransportError(result);
            return;
        }

        write_done_ += sent;
    }

    onWriteCompleted(std::error_code(), write_size_);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::onTransportError(CURLcode result)
{
    long os_error = 0;
    curl_easy_getinfo(curl_.get(), CURLINFO_OS_ERRNO, &os_error);

    CLOG(ERROR) << "Transport error:" << curl_easy_strerror(result) << "(" << os_error << ")";

    // The error of the socket tells more than the code of curl, for example a reset by the peer.
    if (os_error)
        onErrorOccurred(FROM_HERE, std::error_code(static_cast<int>(os_error), asio::system_category()));
    else
        onErrorOccurred(FROM_HERE, ErrorCode::NETWORK_ERROR);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::postError(ErrorCode error_code)
{
    asio::post(io_context_, [this, io = ioState(), error_code]()
    {
        if (io->alive)
            onErrorOccurred(FROM_HERE, error_code);
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelProxy::onDebugMessage(curl_infotype type, std::string_view message)
{
    // The headers sent to the proxy carry its credentials, and the data is ours, so only the progress
    // of curl and the answers of the proxy reach the log.
    if (type != CURLINFO_TEXT && type != CURLINFO_HEADER_IN)
        return;

    while (message.ends_with('\n') || message.ends_with('\r'))
        message.remove_suffix(1);

    if (!message.empty())
        CLOG(INFO) << "curl:" << message;
}
