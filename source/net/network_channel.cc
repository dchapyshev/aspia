//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "net/network_channel.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/unicode.h"
#include "crypto/message_decryptor.h"
#include "crypto/message_encryptor.h"
#include "net/network_channel_proxy.h"
#include "net/network_listener.h"
#include "net/socket_connector.h"
#include "net/socket_reader.h"
#include "net/socket_writer.h"

#if defined(OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

namespace net {

Channel::Channel()
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(io_context_),
      proxy_(new ChannelProxy(this))
{
    connector_ = std::make_unique<SocketConnector>(io_context_);
    connector_->attach(&socket_,
        std::bind(&Channel::onConnected, this),
        std::bind(&Channel::onErrorOccurred, this, std::placeholders::_1));

    init();
}

Channel::Channel(asio::ip::tcp::socket&& socket)
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(std::move(socket)),
      proxy_(new ChannelProxy(this)),
      connected_(true)
{
    DCHECK(socket_.is_open());
    init();
}

Channel::~Channel()
{
    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    listener_ = nullptr;

    if (connector_)
        connector_->dettach();

    reader_->dettach();
    writer_->dettach();

    disconnect();
}

std::shared_ptr<ChannelProxy> Channel::channelProxy()
{
    return proxy_;
}

void Channel::setListener(Listener* listener)
{
    listener_ = listener;
}

void Channel::setEncryptor(std::unique_ptr<crypto::MessageEncryptor> encryptor)
{
    writer_->setEncryptor(std::move(encryptor));
}

void Channel::setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor)
{
    reader_->setDecryptor(std::move(decryptor));
}

std::u16string Channel::peerAddress() const
{
    if (!socket_.is_open())
        return std::u16string();

    return base::utf16FromLocal8Bit(socket_.remote_endpoint().address().to_string());
}

void Channel::connect(std::u16string_view address, uint16_t port)
{
    if (connected_)
        return;

    connector_->connect(address, port);
}

bool Channel::isConnected() const
{
    return connected_;
}

bool Channel::isPaused() const
{
    return reader_->isPaused();
}

void Channel::pause()
{
    reader_->pause();
}

void Channel::resume()
{
    if (!connected_)
        return;

    reader_->resume();
}

void Channel::send(base::ByteArray&& buffer)
{
    return writer_->send(std::move(buffer));
}

bool Channel::setNoDelay(bool enable)
{
    asio::ip::tcp::no_delay option(enable);

    asio::error_code error_code;
    socket_.set_option(option, error_code);

    if (error_code)
    {
        LOG(LS_ERROR) << "Failed to disable Nagle's algorithm: " << error_code.message();
        return false;
    }

    return true;
}

bool Channel::setKeepAlive(bool enable,
                           const std::chrono::milliseconds& time,
                           const std::chrono::milliseconds& interval)
{
#if defined(OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = enable ? TRUE : FALSE;
    alive.keepalivetime = time.count();
    alive.keepaliveinterval = interval.count();

    DWORD bytes_returned;

    if (WSAIoctl(socket_.native_handle(), SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        PLOG(LS_WARNING) << "WSAIoctl failed";
        return false;
    }
#else
    #warning Not implemented
#endif

    return true;
}

void Channel::init()
{
    reader_ = std::make_unique<SocketReader>();
    reader_->attach(&socket_,
        std::bind(&Channel::onMessageReceived, this, std::placeholders::_1),
        std::bind(&Channel::onErrorOccurred, this, std::placeholders::_1));

    writer_ = std::make_unique<SocketWriter>();
    writer_->attach(&socket_,
        std::bind(&Channel::onMessageWritten, this),
        std::bind(&Channel::onErrorOccurred, this, std::placeholders::_1));
}

void Channel::disconnect()
{
    if (!connected_)
        return;

    connected_ = false;

    std::error_code ignored_code;

    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

void Channel::onConnected()
{
    connected_ = true;

    if (listener_)
        listener_->onConnected();
}

void Channel::onErrorOccurred(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    ErrorCode error = ErrorCode::UNKNOWN;

    if (error_code == asio::error::host_not_found)
        error = ErrorCode::SPECIFIED_HOST_NOT_FOUND;
    else if (error_code == asio::error::connection_refused)
        error = ErrorCode::CONNECTION_REFUSED;
    else if (error_code == asio::error::address_in_use)
        error = ErrorCode::ADDRESS_IN_USE;
    else if (error_code == asio::error::timed_out)
        error = ErrorCode::SOCKET_TIMEOUT;
    else if (error_code == asio::error::host_unreachable)
        error = ErrorCode::ADDRESS_NOT_AVAILABLE;
    else if (error_code == asio::error::connection_reset || error_code == asio::error::eof)
        error = ErrorCode::REMOTE_HOST_CLOSED;
    else if (error_code == asio::error::network_down)
        error = ErrorCode::NETWORK_ERROR;

    LOG(LS_WARNING) << "Network error: " << base::utf16FromLocal8Bit(error_code.message())
                    << " (" << error_code.value() << ")";

    disconnect();

    if (listener_)
    {
        listener_->onDisconnected(error);
        listener_ = nullptr;
    }
}

void Channel::onMessageReceived(const base::ByteArray& buffer)
{
    if (listener_)
        listener_->onMessageReceived(buffer);
}

void Channel::onMessageWritten()
{
    if (listener_)
        listener_->onMessageWritten();
}

} // namespace net
