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

#include "base/net/tcp_channel_ng.h"

#include <QThread>

#include <asio/connect.hpp>
#include <asio/ip/address.hpp>
#include <asio/read.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/stream_decryptor.h"
#include "base/crypto/stream_encryptor.h"
#include "base/peer/authenticator.h"
#include "base/threading/asio_event_dispatcher.h"

namespace {

const qint64 kWriteQueueReservedSize = 128;
const qint64 kWritePoolReservedSize = 32;
const TcpChannelNG::Seconds kKeepAliveInterval { 30 };
const TcpChannelNG::Seconds kKeepAliveTimeout { 30 };

//--------------------------------------------------------------------------------------------------
QStringList endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (endpoints.empty())
        return QStringList();

    QStringList list;
    list.reserve(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(QString::fromLocal8Bit(it->endpoint().address().to_string()));

    return list;
}

//--------------------------------------------------------------------------------------------------
void resizeBuffer(QByteArray* buffer, qint64 size)
{
    if (size < 0)
        size = 0;

    if (buffer->capacity() < size)
    {
        buffer->clear();
        buffer->reserve(size);
    }

    buffer->resize(size);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TcpChannelNG::TcpChannelNG(Authenticator* authenticator, QObject* parent)
    : TcpChannel(Type::DIRECT, parent),
      io_context_(AsioEventDispatcher::ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      authenticator_(authenticator)
{
    init();
}

//--------------------------------------------------------------------------------------------------
TcpChannelNG::TcpChannelNG(
    Type type, asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent)
    : TcpChannel(type, parent),
      io_context_(AsioEventDispatcher::ioContext()),
      socket_(std::move(socket)),
      authenticator_(authenticator)
{
    CDCHECK(socket_.is_open());
    init();
    setConnected(true);
}

//--------------------------------------------------------------------------------------------------
TcpChannelNG::~TcpChannelNG()
{
    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    io_->alive = false;
    setConnected(false);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doAuthentication()
{
    if (!authenticator_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return;
    }

    CLOG(TRACE) << "Start authentication";
    authenticator_->start();
    setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::connectTo(const QString& address, quint16 port, const Seconds& timeout)
{
    if (isConnected() || !resolver_)
        return;

    std::string host = address.toLocal8Bit().toStdString();
    std::string service = std::to_string(port);

    // Watchdog for the entire connect phase (resolve + TCP handshake). Without it the channel
    // would wait for the OS-level SYN-retransmit timeout (~2 minutes on Linux) before reporting
    // an unreachable peer.
    SharedPointer<asio::steady_timer> watchdog(new asio::steady_timer(io_context_));
    watchdog->expires_after(timeout);

    auto io = io_;
    watchdog->async_wait([this, io, watchdog](const std::error_code& error_code)
    {
        if (io->alive && !error_code)
            onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    });

    // Fast path for IP addresses. The ASIO resolver serializes all lookups for the io_context through
    // a single background thread, so a literal address could otherwise wait behind slow or failing
    // hostname lookups issued by other channels. Resolving is only needed for host names, so skip it
    // entirely here.
    std::error_code address_error;
    asio::ip::address ip_address = asio::ip::make_address(host, address_error);
    if (!address_error)
    {
        CLOG(TRACE) << "Address is an IP, skipping resolve for" << host << ":" << service;

        asio::ip::tcp::endpoint endpoint(ip_address, port);
        socket_.async_connect(endpoint, [this, io, watchdog, endpoint](
            const std::error_code& error_code)
        {
            if (!io->alive)
                return;

            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;
                watchdog->cancel();
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            CLOG(TRACE) << "Connected to:" << endpoint.address().to_string() << ":" << endpoint.port();
            watchdog->cancel();

            setConnected(true);
            emit sig_connected();

            doAuthentication();
        });
        return;
    }

    CLOG(TRACE) << "Start resolving for" << host << ":" << service;

    resolver_->async_resolve(host, service, [this, io, watchdog](
        const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            watchdog->cancel();
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CLOG(TRACE) << "Resolved:" << endpointsToString(endpoints);

        asio::async_connect(socket_, endpoints,
            [](const std::error_code& error_code, const asio::ip::tcp::endpoint& next)
        {
            if (error_code == asio::error::operation_aborted)
            {
                // If more than one address for a host was resolved, then we return false and cancel
                // attempts to connect to all addresses.
                return false;
            }

            return true;
        },
            [this, io, watchdog](const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
        {
            if (!io->alive)
                return;

            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;
                watchdog->cancel();
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            CLOG(TRACE) << "Connected to:" << endpoint.address().to_string() << ":" << endpoint.port();
            watchdog->cancel();

            setConnected(true);
            emit sig_connected();

            doAuthentication();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::setPaused(bool enable)
{
    if (!isConnected() || paused_ == enable)
        return;

    paused_ = enable;
    if (paused_)
    {
        keep_alive_state_ = KeepAliveState::INACTIVE;
        return;
    }

    // Keep-alive applies only after authentication completes. Before |authenticated_|
    // is set, |encryptor_| may not be ready, and a KEEP_ALIVE frame would be sent
    // unencrypted, which the peer would treat as a protocol violation.
    if (authenticated_)
        startKeepAliveInterval();

    // We already have an incomplete read operation.
    if (state_ == ReadState::READ_HEADER || state_ == ReadState::READ_DATA)
        return;

    // If we have a message that was received before the pause command.
    if (state_ == ReadState::PENDING)
        onMessageReceived();

    doReadHeader();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::send(quint8 channel_id, const QByteArray& buffer)
{
    if (!io_->alive)
        return;

    // USER_DATA may only be sent over an authenticated channel.
    if (!authenticated_)
    {
        CLOG(ERROR) << "Sending message before authentication completed";
        return;
    }

    addWriteTask(USER_DATA, channel_id, buffer, true);
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelNG::setReadBufferSize(int size)
{
    asio::socket_base::receive_buffer_size option(size);

    asio::error_code error_code;
    socket_.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set read buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Read buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelNG::setWriteBufferSize(int size)
{
    asio::socket_base::send_buffer_size option(size);

    asio::error_code error_code;
    socket_.set_option(option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    CLOG(INFO) << "Write buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 TcpChannelNG::pendingBytes() const
{
    qint64 result = 0;

    for (const QByteArray& buffer : std::as_const(io_->write_queue))
        result += buffer.size();

    return result;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::tick(const TimePoint& now)
{
    if (keep_alive_state_ == KeepAliveState::INACTIVE || now < keep_alive_deadline_)
        return;

    if (keep_alive_state_ == KeepAliveState::WAIT_INTERVAL)
    {
        // Incoming traffic since last check or pending outgoing data both prove the
        // connection is alive. Skip the PING and wait out the next interval.
        if (rx_since_last_check_ || !io_->write_queue.isEmpty())
        {
            startKeepAliveInterval();
            return;
        }

        // Channel is idle. Send a PING and require a PONG within kKeepAliveTimeout.
        keep_alive_state_ = KeepAliveState::WAIT_PONG;
        keep_alive_deadline_ = now + kKeepAliveTimeout;
        addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PING, keep_alive_counter_, true);
    }
    else
    {
        // No PONG arrived within the specified period. Forcibly terminate the connection.
        onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::init()
{
    static_assert(sizeof(Header) == 8, "Header must be 8 bytes without padding");

    CCHECK(authenticator_);
    authenticator_->setParent(this);

    write_pool_.reserve(kWritePoolReservedSize);
    io_->write_queue.reserve(kWriteQueueReservedSize);

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());

    connect(authenticator_, &Authenticator::sig_outgoingMessage,
            this, [this](const QByteArray& data, bool encrypted)
    {
        addWriteTask(AUTH_DATA, 0, data, encrypted);
    });

    connect(authenticator_, &Authenticator::sig_keyChanged, this, [this]()
    {
        encryptor_ = StreamEncryptor::createForAes256Gcm(
            authenticator_->sessionKey(Authenticator::Direction::ENCRYPT),
            authenticator_->iv(Authenticator::Direction::ENCRYPT));
        decryptor_ = StreamDecryptor::createForAes256Gcm(
            authenticator_->sessionKey(Authenticator::Direction::DECRYPT),
            authenticator_->iv(Authenticator::Direction::DECRYPT));

        if (!encryptor_ || !decryptor_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }
    });

    connect(authenticator_, &Authenticator::sig_finished, this, [this](Authenticator::ErrorCode error_code)
    {
        setPaused(true);

        switch (error_code)
        {
            case Authenticator::ErrorCode::SUCCESS:
                break;
            case Authenticator::ErrorCode::PROTOCOL_ERROR:
                onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                return;
            case Authenticator::ErrorCode::SESSION_DENIED:
                onErrorOccurred(FROM_HERE, ErrorCode::SESSION_DENIED);
                return;
            case Authenticator::ErrorCode::VERSION_ERROR:
                onErrorOccurred(FROM_HERE, ErrorCode::VERSION_ERROR);
                return;
            case Authenticator::ErrorCode::ACCESS_DENIED:
                onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
                return;
            default:
                onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
                return;
        }

        version_       = authenticator_->peerVersion();
        os_name_       = authenticator_->peerOsName();
        computer_name_ = authenticator_->peerComputerName();
        display_name_  = authenticator_->peerDisplayName();
        architecture_  = authenticator_->peerArch();
        user_name_     = authenticator_->userName();
        user_id_       = authenticator_->userId();
        session_type_  = authenticator_->sessionType();

        authenticator_->disconnect();
        authenticated_ = true;

        emit sig_authenticated();

        // Delete (deleteLater) the object after emitting a signal indicating successful
        // authentication. Do not change this order!
        authenticator_.reset();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::setConnected(bool connected)
{
    connected_ = connected;

    if (!connected_)
    {
        if (resolver_)
        {
            resolver_->cancel();
            resolver_.reset();
        }

        if (socket_.is_open())
        {
            std::error_code ignored_code;
            socket_.cancel(ignored_code);
            socket_.close(ignored_code);
        }

        keep_alive_state_ = KeepAliveState::INACTIVE;
        return;
    }

    asio::error_code error_code;

    asio::ip::tcp::no_delay no_delay_option(true);
    socket_.set_option(no_delay_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Nagle's algorithm is disabled";

    asio::socket_base::receive_buffer_size receive_option;
    socket_.get_option(receive_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Read buffer size:" << receive_option.value();

    asio::socket_base::send_buffer_size send_option;
    socket_.get_option(send_option, error_code);
    if (!error_code)
        CLOG(TRACE) << "Write buffer size:" << send_option.value();

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            CLOG(ERROR) << "Unable to get peer address:" << error_code;
        }
        else
        {
            asio::ip::address address = endpoint.address();
            if (address.is_v4())
            {
                asio::ip::address_v4 ipv4_address = address.to_v4();
                address_ = ipv4_address.to_string();
            }
            else
            {
                asio::ip::address_v6 ipv6_address = address.to_v6();
                if (ipv6_address.is_v4_mapped())
                {
                    asio::ip::address_v4 ipv4_address =
                        asio::ip::make_address_v4(asio::ip::v4_mapped, ipv6_address);
                    address_ =  ipv4_address.to_string();
                }
                else
                {
                    address_ = ipv6_address.to_string();
                }
            }
        }
    }
    catch (const std::error_code& error_code)
    {
        CLOG(ERROR) << "Unable to get peer address:" << error_code;
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
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
#if defined(Q_OS_WINDOWS)
    // asio puts Win32 system errors (e.g. ERROR_SEM_TIMEOUT - kernel-level TCP keep-alive timeout)
    // into asio::system_category(), which is distinct from std::system_category(). Match against
    // the asio category explicitly.
    else if (error_code.category() == asio::system_category() && error_code.value() == ERROR_SEM_TIMEOUT)
        error = ErrorCode::SOCKET_TIMEOUT;
#endif

    CLOG(TRACE) << "Asio error:" << error_code;
    onErrorOccurred(location, error);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    CLOG(TRACE) << "Connection finished:" << error_code << "from" << location;

    if (!io_->alive)
        return;

    if (authenticator_)
    {
        user_name_ = authenticator_->userName();
        user_id_   = authenticator_->userId();
    }

    setConnected(false);

    io_->alive = false;
    emit sig_errorOccurred(error_code);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::onMessageReceived()
{
    if (io_->read_buffer.isEmpty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (io_->read_buffer.size() > kMaxMessageSize ||
        io_->read_buffer.size() != qsizetype(io_->read_header.length))
    {
        CLOG(INFO) << "Invalid message length:" << io_->read_header.length;
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (io_->read_header.type == AUTH_DATA)
    {
        if (!authenticator_ || authenticated_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        if (!decryptor_)
        {
            authenticator_->onIncomingMessage(io_->read_buffer);
            return;
        }

        resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(io_->read_buffer.size()));

        if (!decryptor_->decrypt(io_->read_buffer.data(), io_->read_buffer.size(), // Data
                                 &io_->read_header, sizeof(io_->read_header), // AAD
                                 decrypt_buffer_.data()))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
            return;
        }

        authenticator_->onIncomingMessage(decrypt_buffer_);
        return;
    }

    // Any non-AUTH_DATA message before authentication completes is a protocol violation.
    if (!authenticated_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    // Defense in depth: |decryptor_| must be created before |authenticated_| is set
    // (in Authenticator::sig_keyChanged handler), but guard against any state inconsistency.
    if (!decryptor_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
        return;
    }

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(io_->read_buffer.size()));

    if (!decryptor_->decrypt(io_->read_buffer.data(), io_->read_buffer.size(), // Data
                             &io_->read_header, sizeof(io_->read_header), // AAD
                             decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
        return;
    }

    // Any successfully decrypted message proves the peer is alive. Cheap flag instead of
    // rescheduling the timer on every message - the keep-alive callbacks will consult it
    // when they actually fire.
    rx_since_last_check_ = true;

    if (io_->read_header.type == USER_DATA)
    {
        emit sig_messageReceived(io_->read_header.param1, decrypt_buffer_);
    }
    else if (io_->read_header.type == KEEP_ALIVE)
    {
        if (io_->read_header.param1 & KEEP_ALIVE_PING)
        {
            addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PONG, decrypt_buffer_, true);
            return;
        }

        if (decrypt_buffer_.size() != keep_alive_counter_.size())
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        // Pong must contain the same data as ping.
        if (decrypt_buffer_ != keep_alive_counter_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        // Increase the counter of sent packets.
        largeNumberIncrement(&keep_alive_counter_);

        // PONG received - switch back to the interval cycle.
        startKeepAliveInterval();
    }
    else
    {
        // Ignore unknown types.
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::addWriteTask(quint8 type, quint8 param, const QByteArray& data, bool encrypted)
{
    if (data.isEmpty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    // Only AUTH_DATA may bypass the authenticated state (it carries the handshake itself).
    if (type != AUTH_DATA && !authenticated_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    // Encryption decision is per-task: the authenticator marks individual handshake frames
    // as plaintext or encrypted, and USER_DATA / KEEP_ALIVE are always encrypted.
    if (encrypted && !encryptor_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
        return;
    }

    const qint64 target_data_size = encrypted ?
        encryptor_->encryptedDataSize(data.size()) : data.size();
    const qint64 total_size = target_data_size + static_cast<qint64>(sizeof(Header));
    const qint64 max_size = (type == AUTH_DATA && !authenticated_) ?
        kMaxAuthMessageSize : kMaxMessageSize;

    if (total_size > max_size)
    {
        CLOG(ERROR) << "Too big outgoing message:" << total_size << "(limit:" << max_size << ")";
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    Header header;
    header.type = type;
    header.param1 = param;
    header.param2 = 0;
    header.param3 = 0;
    header.length = static_cast<quint32>(target_data_size);

    QByteArray write_buffer;

    if (!write_pool_.isEmpty())
    {
        write_buffer = std::move(write_pool_.front());
        write_pool_.pop_front();

        if (write_buffer.capacity() < total_size)
            write_buffer.reserve(total_size);
    }

    write_buffer.resize(total_size);

    memcpy(write_buffer.data(), &header, sizeof(Header));

    if (encrypted)
    {
        if (!encryptor_->encrypt(data.data(), data.size(), // Data
                                 &header, sizeof(Header), // AAD
                                 write_buffer.data() + sizeof(Header)))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
            return;
        }
    }
    else
    {
        memcpy(write_buffer.data() + sizeof(Header), data.data(), data.size());
    }

    const bool schedule_write = io_->write_queue.isEmpty();
    io_->write_queue.emplace_back(std::move(write_buffer));
    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doWrite()
{
    const QByteArray& buffer = io_->write_queue.front();

    auto io = io_;
    asio::async_write(socket_,
        asio::buffer(buffer.data(), buffer.size()),
        [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addTxBytes(bytes_transferred); // Update TX statistics.
        CDCHECK(!io_->write_queue.isEmpty());

        if (write_pool_.size() < kWritePoolReservedSize)
            write_pool_.emplace_back(std::move(io_->write_queue.front()));
        io_->write_queue.pop_front();

        if (!io_->write_queue.isEmpty())
            doWrite();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doReadHeader()
{
    state_ = ReadState::READ_HEADER;

    auto io = io_;
    asio::async_read(socket_, asio::mutable_buffer(&io_->read_header, sizeof(Header)),
        [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addRxBytes(bytes_transferred); // Update RX statistics.

        // Until the handshake completes a peer is anonymous, so an inflated header.length
        // would let it pin |kMaxMessageSize| of memory per channel before authenticating.
        // Enforce a much tighter cap for AUTH_DATA - real handshake frames stay well below it.
        const quint32 max_length = authenticated_ ? kMaxMessageSize : kMaxAuthMessageSize;
        if (io_->read_header.length > max_length)
        {
            CLOG(ERROR) << "Too big incoming message:" << io_->read_header.length
                        << "(limit:" << max_length << ")";
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        if (io_->read_header.length)
            doReadData();
        else
            doReadHeader();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doReadData()
{
    resizeBuffer(&io_->read_buffer, io_->read_header.length);

    state_ = ReadState::READ_DATA;

    auto io = io_;
    asio::async_read(socket_, asio::buffer(io_->read_buffer.data(), io_->read_buffer.size()),
        [this, io](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!io->alive)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addRxBytes(bytes_transferred); // Update RX statistics.
        CDCHECK_EQ(bytes_transferred, io_->read_buffer.size());

        if (paused_)
        {
            state_ = ReadState::PENDING;
            return;
        }

        onMessageReceived();

        if (paused_)
        {
            state_ = ReadState::IDLE;
            return;
        }

        doReadHeader();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::startKeepAliveInterval()
{
    rx_since_last_check_ = false;
    keep_alive_state_ = KeepAliveState::WAIT_INTERVAL;
    keep_alive_deadline_ = Clock::now() + kKeepAliveInterval;
}
