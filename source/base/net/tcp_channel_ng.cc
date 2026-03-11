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

#include <QTimer>
#include <QThread>

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"
#include "base/peer/authenticator.h"

namespace base {

namespace {

const int kWriteQueueReservedSize = 128;
const TcpChannelNG::Seconds kKeepAliveInterval { 60 };
const TcpChannelNG::Seconds kKeepAliveTimeout { 30 };

//--------------------------------------------------------------------------------------------------
QStringList endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (endpoints.empty())
        return QStringList();

    QStringList list;
    list.resize(endpoints.size());

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end; ++it)
        list.emplace_back(QString::fromLocal8Bit(it->endpoint().address().to_string()));

    return list;
}

//--------------------------------------------------------------------------------------------------
void resizeBuffer(QByteArray* buffer, qint64 size)
{
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
    : TcpChannel(parent),
      io_context_(base::AsioEventDispatcher::ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      authenticator_(authenticator)
{
    init();
}

//--------------------------------------------------------------------------------------------------
TcpChannelNG::TcpChannelNG(
    asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent)
    : TcpChannel(parent),
      io_context_(base::AsioEventDispatcher::ioContext()),
      socket_(std::move(socket)),
      authenticator_(authenticator)
{
    DCHECK(socket_.is_open());
    init();
    setConnected(true);
}

//--------------------------------------------------------------------------------------------------
TcpChannelNG::~TcpChannelNG()
{
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

    LOG(INFO) << "Start authentication";
    authenticator_->start();
    setPaused(false);
}

//--------------------------------------------------------------------------------------------------
QString TcpChannelNG::peerAddress() const
{
    if (!socket_.is_open() || !isConnected())
        return QString();

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            LOG(ERROR) << "Unable to get peer address:" << error_code;
            return QString();
        }

        asio::ip::address address = endpoint.address();
        if (address.is_v4())
        {
            asio::ip::address_v4 ipv4_address = address.to_v4();
            return QString::fromLocal8Bit(ipv4_address.to_string().c_str());
        }
        else
        {
            asio::ip::address_v6 ipv6_address = address.to_v6();
            if (ipv6_address.is_v4_mapped())
            {
                asio::ip::address_v4 ipv4_address =
                    asio::ip::make_address_v4(asio::ip::v4_mapped, ipv6_address);
                return QString::fromLocal8Bit(ipv4_address.to_string().c_str());
            }
            else
            {
                return QString::fromLocal8Bit(ipv6_address.to_string().c_str());
            }
        }
    }
    catch (const std::error_code& error_code)
    {
        LOG(ERROR) << "Unable to get peer address:" << error_code;
        return QString();
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::connectTo(const QString& address, quint16 port)
{
    if (isConnected() || !resolver_)
        return;

    std::string host = address.toLocal8Bit().toStdString();
    std::string service = std::to_string(port);

    LOG(INFO) << "Start resolving for" << host << ":" << service;

    resolver_->async_resolve(host, service,
        [this](const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

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
            [this](const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(INFO) << "Connected to endpoint:" << endpoint.address().to_string() << ":" << endpoint.port();

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
        keep_alive_timer_->stop();
        return;
    }

    keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    keep_alive_timer_->start(kKeepAliveInterval);

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
    addWriteTask(USER_DATA, channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelNG::setReadBufferSize(int size)
{
    asio::socket_base::receive_buffer_size option(size);

    asio::error_code error_code;
    socket_.set_option(option, error_code);
    if (error_code)
    {
        LOG(ERROR) << "Failed to set read buffer size:" << error_code;
        return false;
    }

    LOG(INFO) << "Read buffer size is changed:" << size;
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
        LOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    LOG(INFO) << "Write buffer size is changed:" << size;
    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 TcpChannelNG::pendingBytes() const
{
    qint64 result = 0;

    for (const auto& task : std::as_const(write_queue_))
        result += task.data().size();

    return result;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::init()
{
    CHECK(authenticator_);
    authenticator_->setParent(this);

    write_queue_.reserve(kWriteQueueReservedSize);

    keep_alive_timer_ = new QTimer(this);
    keep_alive_timer_->setSingleShot(true);

    connect(keep_alive_timer_, &QTimer::timeout, this, &TcpChannelNG::onKeepAliveTimer);

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());

    connect(authenticator_, &Authenticator::sig_outgoingMessage, this, [this](const QByteArray& data)
    {
        addWriteTask(AUTH_DATA, 0, data);
    });

    connect(authenticator_, &Authenticator::sig_keyChanged, this, [this]()
    {
        if (authenticator_->encryption() == proto::key_exchange::ENCRYPTION_AES256_GCM)
        {
            encryptor_ = MessageEncryptor::createForAes256Gcm(
                authenticator_->sessionKey(), authenticator_->encryptIv());
            decryptor_ = MessageDecryptor::createForAes256Gcm(
                authenticator_->sessionKey(), authenticator_->decryptIv());
        }
        else
        {
            DCHECK_EQ(authenticator_->encryption(), proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305);

            encryptor_ = MessageEncryptor::createForChaCha20Poly1305(
                authenticator_->sessionKey(), authenticator_->encryptIv());
            decryptor_ = MessageDecryptor::createForChaCha20Poly1305(
                authenticator_->sessionKey(), authenticator_->decryptIv());
        }

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
        session_type_  = authenticator_->sessionType();

        authenticator_->disconnect();
        authenticator_->deleteLater();
        authenticator_ = nullptr;

        authenticated_ = true;
        emit sig_authenticated();
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

        keep_alive_timer_->stop();
        return;
    }

    asio::error_code error_code;

    asio::ip::tcp::no_delay no_delay_option(true);
    socket_.set_option(no_delay_option, error_code);
    if (!error_code)
        LOG(INFO) << "Nagle's algorithm is disabled";

    asio::socket_base::receive_buffer_size receive_option;
    socket_.get_option(receive_option, error_code);
    if (!error_code)
        LOG(INFO) << "Read buffer size:" << receive_option.value();

    asio::socket_base::send_buffer_size send_option;
    socket_.get_option(send_option, error_code);
    if (!error_code)
        LOG(INFO) << "Write buffer size:" << send_option.value();
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

    LOG(ERROR) << "Asio error:" << error_code;
    onErrorOccurred(location, error);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    LOG(ERROR) << "Connection finished:" << error_code << "from" << location;
    setConnected(false);
    emit sig_errorOccurred(error_code);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::onMessageReceived()
{
    if (read_buffer_.isEmpty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (read_buffer_.size() > kMaxMessageSize || read_buffer_.size() != read_header_.length)
    {
        LOG(INFO) << "Invalid message length:" << read_header_.length;
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (read_header_.type == AUTH_DATA)
    {
        if (!authenticator_ || authenticated_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
            return;
        }

        if (!decryptor_)
        {
            authenticator_->onIncomingMessage(read_buffer_);
            return;
        }

        resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_buffer_.size()));

        if (!decryptor_->decrypt(read_buffer_.data(), read_buffer_.size(), // Data
                                 &read_header_, sizeof(read_header_), // AAD
                                 decrypt_buffer_.data()))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
            return;
        }

        authenticator_->onIncomingMessage(decrypt_buffer_);
        return;
    }

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_buffer_.size()));

    if (!decryptor_->decrypt(read_buffer_.data(), read_buffer_.size(), // Data
                             &read_header_, sizeof(read_header_), // AAD
                             decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
        return;
    }

    if (read_header_.type == USER_DATA)
    {
        emit sig_messageReceived(read_header_.param1, decrypt_buffer_);
    }
    else if (read_header_.type == KEEP_ALIVE)
    {
        if (read_header_.param1 & KEEP_ALIVE_PING)
        {
            addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PONG, decrypt_buffer_);
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

        // Restart keep alive timer.
        keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
        keep_alive_timer_->start(kKeepAliveInterval);
    }
    else
    {
        // Ignore unknown types.
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::addWriteTask(quint8 type, quint8 param, const QByteArray& data)
{
    const bool schedule_write = write_queue_.isEmpty();
    write_queue_.emplace_back(type, param, data);
    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doWrite()
{
    const WriteTask& task = write_queue_.front();
    const QByteArray& source_buffer = task.data();

    if (source_buffer.isEmpty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    qint64 target_data_size =
        encryptor_ ? encryptor_->encryptedDataSize(source_buffer.size()) : source_buffer.size();

    resizeBuffer(&write_buffer_, target_data_size + sizeof(Header));

    if (write_buffer_.size() > kMaxMessageSize)
    {
        LOG(ERROR) << "Too big outgoing message:" << target_data_size;
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    Header* header = reinterpret_cast<Header*>(write_buffer_.data());
    header->type = task.type();
    header->param1 = task.param();
    header->param2 = 0;
    header->param3 = 0;
    header->length = static_cast<quint32>(target_data_size);

    if (encryptor_)
    {
        if (!encryptor_->encrypt(source_buffer.data(), source_buffer.size(), // Data
                                 header, sizeof(Header), // AAD
                                 write_buffer_.data() + sizeof(Header)))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
            return;
        }
    }
    else
    {
        memcpy(write_buffer_.data() + sizeof(Header), source_buffer.data(), source_buffer.size());
    }

    asio::async_write(socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addTxBytes(bytes_transferred); // Update TX statistics.
        DCHECK(!write_queue_.empty());

        const WriteTask& task = write_queue_.front();
        quint8 type = task.type();
        quint8 param = task.param();

        write_queue_.pop_front();

        bool schedule_write = !write_queue_.isEmpty();

        if (type == USER_DATA)
        {
            emit sig_messageWritten(param);
        }
        else if (type == AUTH_DATA)
        {
            if (!authenticator_ || authenticated_)
            {
                onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
                return;
            }

            authenticator_->onMessageWritten();
        }

        if (schedule_write)
            doWrite();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doReadHeader()
{
    state_ = ReadState::READ_HEADER;
    asio::async_read(socket_, asio::mutable_buffer(&read_header_, sizeof(Header)),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addRxBytes(bytes_transferred); // Update RX statistics.

        if (read_header_.length > kMaxMessageSize)
        {
            LOG(ERROR) << "Too big incoming message:" << read_header_.length;
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        if (read_header_.length)
            doReadData();
        else
            doReadHeader();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelNG::doReadData()
{
    resizeBuffer(&read_buffer_, read_header_.length);

    state_ = ReadState::READ_DATA;
    asio::async_read(socket_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        addRxBytes(bytes_transferred); // Update RX statistics.
        DCHECK_EQ(bytes_transferred, read_buffer_.size());

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
void TcpChannelNG::onKeepAliveTimer()
{
    if (keep_alive_timer_type_ == KEEP_ALIVE_INTERVAL)
    {
        // If a response is not received within the specified interval, the connection will be terminated.
        addWriteTask(KEEP_ALIVE, KEEP_ALIVE_PING, keep_alive_counter_);
        keep_alive_timer_type_ = KEEP_ALIVE_TIMEOUT;
        keep_alive_timer_->start(kKeepAliveTimeout);
    }
    else
    {
        // No response came within the specified period of time. We forcibly terminate the connection.
        DCHECK_EQ(keep_alive_timer_type_, KEEP_ALIVE_TIMEOUT);
        onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    }
}

} // namespace base
