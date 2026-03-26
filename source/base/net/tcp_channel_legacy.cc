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

#include "base/net/tcp_channel_legacy.h"

#include <QThread>

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include "base/asio_event_dispatcher.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/stream_decryptor.h"
#include "base/crypto/stream_encryptor.h"

namespace base {

namespace {

const int kWriteQueueReservedSize = 128;
const TcpChannelLegacy::Seconds kKeepAliveInterval { 60 };
const TcpChannelLegacy::Seconds kKeepAliveTimeout { 30 };

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
void resizeBuffer(QByteArray* buffer, qint64 new_size)
{
    // If the reserved buffer size is less, then increase it.
    if (buffer->capacity() < new_size)
    {
        buffer->clear();
        buffer->reserve(new_size);
    }

    // Change the size of the buffer.
    buffer->resize(new_size);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TcpChannelLegacy::TcpChannelLegacy(Authenticator* authenticator, QObject* parent)
    : TcpChannel(Type::DIRECT, parent),
      io_context_(base::AsioEventDispatcher::ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      authenticator_(authenticator)
{
    init();
}

//--------------------------------------------------------------------------------------------------
TcpChannelLegacy::TcpChannelLegacy(
    Type type, asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent)
    : TcpChannel(type, parent),
      io_context_(base::AsioEventDispatcher::ioContext()),
      socket_(std::move(socket)),
      authenticator_(authenticator)
{
    CDCHECK(socket_.is_open());
    init();
    setConnected(true);
}

//--------------------------------------------------------------------------------------------------
TcpChannelLegacy::~TcpChannelLegacy()
{
    // Mark guard before releasing resources so that any pending ASIO handlers
    // (already completed but not yet dispatched) will see the object is gone.
    *alive_guard_ = false;
    disconnectFrom();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doAuthentication()
{
    if (!authenticator_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return;
    }

    CLOG(INFO) << "Start authentication";
    authenticator_->start();
    setPaused(false);
}

//--------------------------------------------------------------------------------------------------
QString TcpChannelLegacy::peerAddress() const
{
    if (!socket_.is_open() || !isConnected())
        return QString();

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            CLOG(ERROR) << "Unable to get peer address:" << error_code;
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
        CLOG(ERROR) << "Unable to get peer address:" << error_code;
        return QString();
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::connectTo(const QString& address, quint16 port)
{
    if (isConnected() || !resolver_)
        return;

    std::string host = address.toLocal8Bit().toStdString();
    std::string service = std::to_string(port);

    CLOG(INFO) << "Start resolving for" << host << ":" << service;

    auto guard = alive_guard_;
    resolver_->async_resolve(host, service,
        [this, guard](const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CLOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);

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
            [this, guard](const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
        {
            if (!*guard)
                return;

            if (error_code)
            {
                if (error_code == asio::error::operation_aborted)
                    return;

                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            CLOG(INFO) << "Connected to endpoint:" << endpoint.address().to_string() << ":"
                       << endpoint.port();

            setConnected(true);
            emit sig_connected();

            doAuthentication();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::setPaused(bool enable)
{
    if (!isConnected() || paused_ == enable)
        return;

    paused_ = enable;
    if (paused_)
        return;

    switch (state_)
    {
        // We already have an incomplete read operation.
        case ReadState::READ_SIZE:
        case ReadState::READ_USER_DATA:
        case ReadState::READ_SERVICE_HEADER:
        case ReadState::READ_SERVICE_DATA:
            return;

        default:
            break;
    }

    // If we have a message that was received before the pause command.
    if (state_ == ReadState::PENDING)
        onMessageReceived();

    doReadSize();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::send(quint8 channel_id, const QByteArray& buffer)
{
    addWriteTask(WriteTask::Type::USER_DATA, channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelLegacy::setReadBufferSize(int size)
{
    asio::socket_base::receive_buffer_size new_option(size);

    asio::error_code error_code;
    socket_.set_option(new_option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set read buffer size:" << error_code;
        return false;
    }

    asio::socket_base::receive_buffer_size current_option;
    socket_.get_option(current_option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to get read buffer size:" << error_code;
        return false;
    }

    // The operating system may adjust the buffer size at its discretion.
    if (current_option.value() != size)
    {
        CLOG(WARNING) << "Read buffer size set successfully, but actual size:"
                      << current_option.value() << "expected:" << size;
    }
    else
    {
        CLOG(INFO) << "Read buffer size is changed:" << size;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannelLegacy::setWriteBufferSize(int size)
{
    asio::socket_base::send_buffer_size new_option(size);

    asio::error_code error_code;
    socket_.set_option(new_option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    asio::socket_base::send_buffer_size current_option;
    socket_.get_option(current_option, error_code);
    if (error_code)
    {
        CLOG(ERROR) << "Failed to set write buffer size:" << error_code;
        return false;
    }

    if (current_option.value() != size)
    {
        CLOG(WARNING) << "Write buffer size set successfully, but actual size:"
                      << current_option.value() << "expected:" << size;
    }
    else
    {
        CLOG(INFO) << "Write buffer size is changed:" << size;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 TcpChannelLegacy::pendingBytes() const
{
    qint64 result = 0;

    for (const auto& task : std::as_const(write_queue_))
        result += task.data().size();

    return result;
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::disconnectFrom()
{
    setConnected(false);

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
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::init()
{
    write_queue_.reserve(kWriteQueueReservedSize);

    keep_alive_timer_ = new QTimer(this);
    keep_alive_timer_->setSingleShot(true);

    connect(keep_alive_timer_, &QTimer::timeout, this, &TcpChannelLegacy::onKeepAliveTimer);

    if (authenticator_)
    {
        authenticator_->setParent(this);
        connect(authenticator_, &Authenticator::sig_outgoingMessage, this, &TcpChannelLegacy::onAuthenticatorMessage);
        connect(authenticator_, &Authenticator::sig_keyChanged, this, &TcpChannelLegacy::onKeyChanged);
        connect(authenticator_, &Authenticator::sig_finished, this, &TcpChannelLegacy::onAuthenticatorFinished);
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::setConnected(bool connected)
{
    connected_ = connected;

    if (!connected_)
        return;

    asio::ip::tcp::no_delay no_delay_option(true);

    asio::error_code error_code;
    socket_.set_option(no_delay_option, error_code);
    if (error_code)
        CLOG(ERROR) << "Failed to disable Nagle's algorithm:" << error_code;
    else
        CLOG(INFO) << "Nagle's algorithm is disabled";

    asio::socket_base::receive_buffer_size receive_option;
    socket_.get_option(receive_option, error_code);
    if (error_code)
        CLOG(ERROR) << "Failed to get read buffer size:" << error_code;
    else
        CLOG(INFO) << "Read buffer size:" << receive_option.value();

    asio::socket_base::send_buffer_size send_option;
    socket_.get_option(send_option, error_code);
    if (error_code)
        CLOG(ERROR) << "Failed to get write buffer size:" << error_code;
    else
        CLOG(INFO) << "Write buffer size:" << send_option.value();

    keep_alive_counter_.resize(sizeof(quint32));
    memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());

    CLOG(INFO) << "Starting keep alive timer";

    keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    keep_alive_timer_->start(kKeepAliveInterval);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onKeyChanged()
{
    if (!authenticator_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
        return;
    }

    if (authenticator_->encryption() == proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        encryptor_ = StreamEncryptor::createForAes256Gcm(
            authenticator_->sessionKey(), authenticator_->encryptIv());
        decryptor_ = StreamDecryptor::createForAes256Gcm(
            authenticator_->sessionKey(), authenticator_->decryptIv());
    }
    else
    {
        CDCHECK_EQ(authenticator_->encryption(), proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305);

        encryptor_ = StreamEncryptor::createForChaCha20Poly1305(
            authenticator_->sessionKey(), authenticator_->encryptIv());
        decryptor_ = StreamDecryptor::createForChaCha20Poly1305(
            authenticator_->sessionKey(), authenticator_->decryptIv());
    }

    if (!encryptor_ || !decryptor_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onAuthenticatorMessage(const QByteArray& data)
{
    send(0, data);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onAuthenticatorFinished(Authenticator::ErrorCode error_code)
{
    setPaused(true);

    if (!authenticator_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

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

    is_channel_id_supported_ = true;
    CLOG(INFO) << "Support for channel id is enabled";

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
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onErrorOccurred(const Location& location, const std::error_code& error_code)
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

    CLOG(ERROR) << "Asio error:" << error_code;
    onErrorOccurred(location, error);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    CLOG(ERROR) << "Connection finished with error" << error_code << "from" << location;

    if (authenticator_ && error_code == ErrorCode::CRYPTO_ERROR)
    {
        CLOG(ERROR) << "Invalid key or username/password";
        error_code = ErrorCode::ACCESS_DENIED;
    }

    disconnectFrom();
    emit sig_errorOccurred(error_code);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onMessageWritten(quint8 channel_id)
{
    if (!authenticated_)
    {
        if (!authenticator_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
            return;
        }

        authenticator_->onMessageWritten();
        return;
    }

    emit sig_messageWritten(channel_id);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onMessageReceived()
{
    char* read_data = read_buffer_.data();
    size_t read_size = read_buffer_.size();

    UserDataHeader header;

    if (is_channel_id_supported_)
    {
        if (read_size < sizeof(header))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        memcpy(&header, read_data, sizeof(header));

        read_data += sizeof(header);
        read_size -= sizeof(header);
    }
    else
    {
        memset(&header, 0, sizeof(header));
    }

    if (!read_size)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    auto on_autenticator_message = [this](const char* data, size_t size)
    {
        if (!authenticator_)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::UNKNOWN);
            return;
        }

        authenticator_->onIncomingMessage(QByteArray::fromRawData(data, size));
    };

    if (!decryptor_)
    {
        on_autenticator_message(read_data, read_size);
        return;
    }

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_size));

    if (!decryptor_->decrypt(read_data, read_size, decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
        return;
    }

    if (!authenticated_)
    {
        on_autenticator_message(decrypt_buffer_.data(), decrypt_buffer_.size());
        return;
    }

    emit sig_messageReceived(header.channel_id, decrypt_buffer_);
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::addWriteTask(WriteTask::Type type, quint8 channel_id, const QByteArray& data)
{
    const bool schedule_write = write_queue_.isEmpty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace_back(type, channel_id, data);

    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doWrite()
{
    const WriteTask& task = write_queue_.front();
    const QByteArray& source_buffer = task.data();
    const quint8 channel_id = task.channelId();

    if (source_buffer.isEmpty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (task.type() == WriteTask::Type::USER_DATA)
    {
        size_t target_data_size;

        if (encryptor_)
        {
            // Calculate the size of the encrypted message.
            target_data_size = encryptor_->encryptedDataSize(source_buffer.size());
        }
        else
        {
            target_data_size = source_buffer.size();
        }

        if (is_channel_id_supported_)
            target_data_size += sizeof(UserDataHeader);

        if (target_data_size > kMaxMessageSize)
        {
            CLOG(ERROR) << "Too big outgoing message:" << target_data_size;
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        asio::const_buffer variable_size = variable_size_writer_.variableSize(target_data_size);

        resizeBuffer(&write_buffer_, variable_size.size() + target_data_size);

        // Copy the size of the message to the buffer.
        memcpy(write_buffer_.data(), variable_size.data(), variable_size.size());

        char* write_buffer = write_buffer_.data() + variable_size.size();
        if (is_channel_id_supported_)
        {
            UserDataHeader header;
            header.channel_id = channel_id;
            header.reserved = 0;

            // Copy the channel id to the buffer.
            memcpy(write_buffer, &header, sizeof(header));
            write_buffer += sizeof(header);
        }

        if (encryptor_)
        {
            // Encrypt the message.
            if (!encryptor_->encrypt(source_buffer.data(), source_buffer.size(), write_buffer))
            {
                onErrorOccurred(FROM_HERE, ErrorCode::CRYPTO_ERROR);
                return;
            }
        }
        else
        {
            memcpy(write_buffer, source_buffer.data(), source_buffer.size());
        }
    }
    else
    {
        CDCHECK_EQ(task.type(), WriteTask::Type::SERVICE_DATA);

        resizeBuffer(&write_buffer_, source_buffer.size());

        // Service data does not need encryption. Copy the source buffer.
        memcpy(write_buffer_.data(), source_buffer.data(), source_buffer.size());
    }

    // Send the buffer to the recipient.
    auto guard = alive_guard_;
    asio::async_write(socket_, asio::buffer(write_buffer_.data(), write_buffer_.size()),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK(!write_queue_.empty());

        // Update TX statistics.
        addTxBytes(bytes_transferred);

        const WriteTask& task = write_queue_.front();
        WriteTask::Type task_type = task.type();
        quint8 channel_id = task.channelId();

        // Delete the sent message from the queue.
        write_queue_.pop_front();

        // If the queue is not empty, then we send the following message.
        bool schedule_write = !write_queue_.empty();

        if (task_type == WriteTask::Type::USER_DATA)
            onMessageWritten(channel_id);

        if (schedule_write)
            doWrite();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doReadSize()
{
    state_ = ReadState::READ_SIZE;

    auto guard = alive_guard_;
    asio::async_read(socket_, variable_size_reader_.buffer(),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        // Update RX statistics.
        addRxBytes(bytes_transferred);

        std::optional<size_t> size = variable_size_reader_.messageSize();
        if (size.has_value())
        {
            size_t message_size = *size;

            if (message_size > kMaxMessageSize)
            {
                CLOG(ERROR) << "Too big incoming message:" << message_size;
                onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                return;
            }

            // If the message size is 0 (in other words, the first received byte is 0), then you need
            // to start reading the service message.
            if (!message_size)
            {
                doReadServiceHeader();
                return;
            }

            doReadUserData(message_size);
        }
        else
        {
            doReadSize();
        }
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doReadUserData(size_t length)
{
    resizeBuffer(&read_buffer_, length);

    state_ = ReadState::READ_USER_DATA;

    auto guard = alive_guard_;
    asio::async_read(socket_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;

            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(state_, ReadState::READ_USER_DATA);

        // Update RX statistics.
        addRxBytes(bytes_transferred);

        CDCHECK_EQ(bytes_transferred, read_buffer_.size());

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

        doReadSize();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doReadServiceHeader()
{
    resizeBuffer(&read_buffer_, sizeof(ServiceHeader));

    state_ = ReadState::READ_SERVICE_HEADER;

    auto guard = alive_guard_;
    asio::async_read(socket_, asio::buffer(read_buffer_.data(), read_buffer_.size()),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
        CDCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
        CDCHECK_EQ(bytes_transferred, read_buffer_.size());

        // Update RX statistics.
        addRxBytes(bytes_transferred);

        ServiceHeader* header = reinterpret_cast<ServiceHeader*>(read_buffer_.data());
        if (header->length > kMaxMessageSize)
        {
            CLOG(INFO) << "Too big service message:" << header->length;
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        if (header->type == KEEP_ALIVE)
        {
            // Keep alive packet must always contain data.
            if (!header->length)
            {
                onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                return;
            }

            doReadServiceData(header->length);
        }
        else
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::doReadServiceData(size_t length)
{
    CDCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
    CDCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    CDCHECK_GT(length, 0u);

    read_buffer_.resize(read_buffer_.size() + static_cast<qsizetype>(length));

    // Now we read the data after the header.
    state_ = ReadState::READ_SERVICE_DATA;

    auto guard = alive_guard_;
    asio::async_read(socket_,
        asio::buffer(read_buffer_.data() + sizeof(ServiceHeader),
                     read_buffer_.size() - sizeof(ServiceHeader)),
        [this, guard](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (!*guard)
            return;

        if (error_code)
        {
            if (error_code == asio::error::operation_aborted)
                return;
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        CDCHECK_EQ(state_, ReadState::READ_SERVICE_DATA);
        CDCHECK_GT(read_buffer_.size(), sizeof(ServiceHeader));

        // Update RX statistics.
        addRxBytes(bytes_transferred);

        // Incoming buffer contains a service header.
        ServiceHeader* header = reinterpret_cast<ServiceHeader*>(read_buffer_.data());

        CDCHECK_EQ(bytes_transferred, read_buffer_.size() - sizeof(ServiceHeader));
        CDCHECK_LE(header->length, kMaxMessageSize);

        if (header->type == KEEP_ALIVE)
        {
            if (header->flags & KEEP_ALIVE_PING)
            {
                // Send pong.
                sendKeepAlive(KEEP_ALIVE_PONG,
                              read_buffer_.data() + sizeof(ServiceHeader),
                              read_buffer_.size() - sizeof(ServiceHeader));
            }
            else
            {
                if (read_buffer_.size() < static_cast<qsizetype>(sizeof(ServiceHeader) + header->length))
                {
                    onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                    return;
                }

                if (header->length != keep_alive_counter_.size())
                {
                    onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                    return;
                }

                // Pong must contain the same data as ping.
                if (memcmp(read_buffer_.data() + sizeof(ServiceHeader),
                           keep_alive_counter_.data(),
                           keep_alive_counter_.size()) != 0)
                {
                    onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
                    return;
                }

                // The user can disable keep alive. Restart the timer only if keep alive is enabled.
                if (keep_alive_timer_->isActive())
                {
                    CDCHECK(!keep_alive_counter_.isEmpty());

                    // Increase the counter of sent packets.
                    largeNumberIncrement(&keep_alive_counter_);

                    // Restart keep alive timer.
                    keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
                    keep_alive_timer_->start(kKeepAliveInterval);
                }
            }
        }
        else
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        doReadSize();
    });
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::onKeepAliveTimer()
{
    if (keep_alive_timer_type_ == KEEP_ALIVE_INTERVAL)
    {
        // Send ping.
        sendKeepAlive(KEEP_ALIVE_PING, keep_alive_counter_.data(), keep_alive_counter_.size());

        // If a response is not received within the specified interval, the connection will be terminated.
        keep_alive_timer_type_ = KEEP_ALIVE_TIMEOUT;
        keep_alive_timer_->start(kKeepAliveTimeout);
    }
    else
    {
        CDCHECK_EQ(keep_alive_timer_type_, KEEP_ALIVE_TIMEOUT);

        // No response came within the specified period of time. We forcibly terminate the connection.
        onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannelLegacy::sendKeepAlive(quint8 flags, const void* data, size_t size)
{
    ServiceHeader header;
    memset(&header, 0, sizeof(header));

    header.type   = KEEP_ALIVE;
    header.flags  = flags;
    header.length = static_cast<quint32>(size);

    QByteArray buffer;
    buffer.resize(static_cast<qsizetype>(sizeof(quint8) + sizeof(header) + size));

    // The first byte set to 0 indicates that this is a service message.
    buffer[0] = 0;

    // Now copy the header and data to the buffer.
    memcpy(buffer.data() + sizeof(quint8), &header, sizeof(header));
    memcpy(buffer.data() + sizeof(quint8) + sizeof(header), data, size);

    // Add a task to the queue.
    addWriteTask(WriteTask::Type::SERVICE_DATA, 0, std::move(buffer));
}

//--------------------------------------------------------------------------------------------------
asio::mutable_buffer TcpChannelLegacy::VariableSizeReader::buffer()
{
    DCHECK_LT(pos_, std::size(buffer_));
    return asio::mutable_buffer(&buffer_[pos_], sizeof(quint8));
}

//--------------------------------------------------------------------------------------------------
std::optional<size_t> TcpChannelLegacy::VariableSizeReader::messageSize()
{
    DCHECK_LT(pos_, std::size(buffer_));

    if (pos_ == 3 || !(buffer_[pos_] & 0x80))
    {
        size_t result = buffer_[0] & 0x7F;

        if (pos_ >= 1)
            result += (buffer_[1] & 0x7F) << 7;

        if (pos_ >= 2)
            result += (buffer_[2] & 0x7F) << 14;

        if (pos_ >= 3)
            result += buffer_[3] << 21;

        memset(buffer_, 0, std::size(buffer_));
        pos_ = 0;

        return result;
    }
    else
    {
        ++pos_;
        return std::nullopt;
    }
}

//--------------------------------------------------------------------------------------------------
asio::const_buffer TcpChannelLegacy::VariableSizeWriter::variableSize(size_t size)
{
    size_t length = 1;

    buffer_[0] = size & 0x7F;
    if (size > 0x7F) // 127 bytes
    {
        buffer_[0] |= 0x80;
        buffer_[length++] = size >> 7 & 0x7F;

        if (size > 0x3FFF) // 16383 bytes
        {
            buffer_[1] |= 0x80;
            buffer_[length++] = size >> 14 & 0x7F;

            if (size > 0x1FFFF) // 2097151 bytes
            {
                buffer_[2] |= 0x80;
                buffer_[length++] = size >> 21 & 0xFF;
            }
        }
    }

    return asio::const_buffer(buffer_, length);
}

} // namespace base
