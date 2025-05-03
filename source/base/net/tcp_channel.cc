//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/tcp_channel.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_encryptor_fake.h"
#include "base/crypto/message_decryptor_fake.h"
#include "base/net/tcp_channel_proxy.h"
#include "base/strings/unicode.h"
#include "base/threading/asio_event_dispatcher.h"
#include "base/threading/thread.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <QThread>

namespace base {

namespace {

std::string endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    std::string str;

    for (auto it = endpoints.begin(); it != endpoints.end();)
    {
        str += it->endpoint().address().to_string();
        if (++it != endpoints.end())
            str += ", ";
    }

    return str;
}

} // namespace

class TcpChannel::Handler
{
public:
    explicit Handler(TcpChannel* channel);
    ~Handler();

    void dettach();

    void onResolved(const std::error_code& error_code,
                    const asio::ip::tcp::resolver::results_type& endpoints);
    void onConnected(const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint);
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);
    void onReadSize(const std::error_code& error_code, size_t bytes_transferred);
    void onReadUserData(const std::error_code& error_code, size_t bytes_transferred);
    void onReadServiceHeader(const std::error_code& error_code, size_t bytes_transferred);
    void onReadServiceData(const std::error_code& error_code, size_t bytes_transferred);
    void onKeepAliveInterval(const std::error_code& error_code);
    void onKeepAliveTimeout(const std::error_code& error_code);

private:
    TcpChannel* channel_;
    DISALLOW_COPY_AND_ASSIGN(Handler);
};

//--------------------------------------------------------------------------------------------------
TcpChannel::Handler::Handler(TcpChannel* channel)
    : channel_(channel)
{
    DCHECK(channel_);
}

//--------------------------------------------------------------------------------------------------
TcpChannel::Handler::~Handler() = default;

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::dettach()
{
    channel_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onResolved(
    const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (channel_)
        channel_->onResolved(error_code, endpoints);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onConnected(
    const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
{
    if (channel_)
        channel_->onConnected(error_code, endpoint);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (channel_)
        channel_->onWrite(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onReadSize(const std::error_code& error_code, size_t bytes_transferred)
{
    if (channel_)
        channel_->onReadSize(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onReadUserData(const std::error_code& error_code, size_t bytes_transferred)
{
    if (channel_)
        channel_->onReadUserData(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onReadServiceHeader(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (channel_)
        channel_->onReadServiceHeader(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onReadServiceData(
    const std::error_code& error_code, size_t bytes_transferred)
{
    if (channel_)
        channel_->onReadServiceData(error_code, bytes_transferred);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onKeepAliveInterval(const std::error_code& error_code)
{
    if (channel_)
        channel_->onKeepAliveInterval(error_code);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::Handler::onKeepAliveTimeout(const std::error_code& error_code)
{
    if (channel_)
        channel_->onKeepAliveTimeout(error_code);
}

//--------------------------------------------------------------------------------------------------
TcpChannel::TcpChannel(QObject* parent)
    : NetworkChannel(parent),
      proxy_(new TcpChannelProxy(base::Thread::currentTaskRunner(), this)),
      io_context_(base::AsioEventDispatcher::currentIoContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      encryptor_(std::make_unique<MessageEncryptorFake>()),
      decryptor_(std::make_unique<MessageDecryptorFake>()),
      handler_(base::make_local_shared<Handler>(this))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TcpChannel::TcpChannel(asio::ip::tcp::socket&& socket, QObject* parent)
    : NetworkChannel(parent),
      proxy_(new TcpChannelProxy(base::Thread::currentTaskRunner(), this)),
      io_context_(base::AsioEventDispatcher::currentIoContext()),
      socket_(std::move(socket)),
      connected_(true),
      encryptor_(std::make_unique<MessageEncryptorFake>()),
      decryptor_(std::make_unique<MessageDecryptorFake>()),
      handler_(base::make_local_shared<Handler>(this))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(socket_.is_open());
}

//--------------------------------------------------------------------------------------------------
TcpChannel::~TcpChannel()
{
    LOG(LS_INFO) << "Dtor (start)";

    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    disconnect();
    LOG(LS_INFO) << "Dtor (end)";
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<TcpChannelProxy> TcpChannel::channelProxy()
{
    return proxy_;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::setEncryptor(std::unique_ptr<MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::setDecryptor(std::unique_ptr<MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

//--------------------------------------------------------------------------------------------------
QString TcpChannel::peerAddress() const
{
    if (!socket_.is_open() || !isConnected())
        return QString();

    try
    {
        std::error_code error_code;
        asio::ip::tcp::endpoint endpoint = socket_.remote_endpoint(error_code);
        if (error_code)
        {
            LOG(LS_ERROR) << "Unable to get peer address: "
                          << base::utf16FromLocal8Bit(error_code.message());
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
        LOG(LS_ERROR) << "Unable to get peer address: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return QString();
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::connect(const QString& address, quint16 port)
{
    if (connected_ || !resolver_)
        return;

    std::string host = address.toLocal8Bit().toStdString();
    std::string service = std::to_string(port);

    LOG(LS_INFO) << "Start resolving for " << host << ":" << service;

    resolver_->async_resolve(host, service,
                             std::bind(&Handler::onResolved,
                                       handler_,
                                       std::placeholders::_1,
                                       std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::isConnected() const
{
    return connected_;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::isPaused() const
{
    return paused_;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::pause()
{
    paused_ = true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::resume()
{
    if (!connected_ || !paused_)
        return;

    paused_ = false;

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
void TcpChannel::send(quint8 channel_id, QByteArray&& buffer)
{
    addWriteTask(WriteTask::Type::USER_DATA, channel_id, std::move(buffer));
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::setNoDelay(bool enable)
{
    asio::ip::tcp::no_delay option(enable);

    asio::error_code error_code;
    socket_.set_option(option, error_code);

    if (error_code)
    {
        LOG(LS_ERROR) << "Failed to disable Nagle's algorithm: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::setKeepAlive(bool enable, const Seconds& interval, const Seconds& timeout)
{
    if (enable && keep_alive_timer_)
    {
        LOG(LS_ERROR) << "Keep alive already active";
        return false;
    }

    if (interval < Seconds(15) || interval > Seconds(300))
    {
        LOG(LS_ERROR) << "Invalid interval: " << interval.count();
        return false;
    }

    if (timeout < Seconds(5) || timeout > Seconds(60))
    {
        LOG(LS_ERROR) << "Invalid timeout: " << timeout.count();
        return false;
    }

    if (!enable)
    {
        LOG(LS_INFO) << "Keep alive disabled";

        keep_alive_counter_.clear();

        if (keep_alive_timer_)
        {
            keep_alive_timer_->cancel();
            keep_alive_timer_.reset();
        }
    }
    else
    {
        LOG(LS_INFO) << "Keep alive enabled (interval=" << interval.count()
                     << " timeout=" << timeout.count() << ")";

        keep_alive_interval_ = interval;
        keep_alive_timeout_ = timeout;

        keep_alive_counter_.resize(sizeof(quint32));
        memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());

        keep_alive_timer_ = std::make_unique<asio::steady_timer>(io_context_);
        keep_alive_timer_->expires_after(keep_alive_interval_);
        keep_alive_timer_->async_wait(
            std::bind(&Handler::onKeepAliveInterval, handler_, std::placeholders::_1));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::setChannelIdSupport(bool enable)
{
    is_channel_id_supported_ = enable;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::hasChannelIdSupport() const
{
    return is_channel_id_supported_;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::setReadBufferSize(size_t size)
{
    asio::socket_base::receive_buffer_size option(static_cast<int>(size));

    asio::error_code error_code;
    socket_.set_option(option, error_code);

    if (error_code)
    {
        LOG(LS_ERROR) << "Failed to set read buffer size: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool TcpChannel::setWriteBufferSize(size_t size)
{
    asio::socket_base::send_buffer_size option(static_cast<int>(size));

    asio::error_code error_code;
    socket_.set_option(option, error_code);

    if (error_code)
    {
        LOG(LS_ERROR) << "Failed to set write buffer size: "
                      << base::utf16FromLocal8Bit(error_code.message());
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::disconnect()
{
    LOG(LS_INFO) << "Disconnect";
    connected_ = false;

    handler_->dettach();

    if (resolver_)
    {
        LOG(LS_INFO) << "Destroy resolver";
        resolver_->cancel();
        resolver_.reset();
    }

    if (socket_.is_open())
    {
        LOG(LS_INFO) << "Cancel async operations";
        std::error_code ignored_code;
        socket_.cancel(ignored_code);

        LOG(LS_INFO) << "Close socket";
        socket_.close(ignored_code);
    }
    else
    {
        LOG(LS_INFO) << "Socket already closed";
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
    {
        LOG(LS_INFO) << "Operation aborted (from: " << location.toString() << ")";
        return;
    }

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

    LOG(LS_ERROR) << "Asio error: " << utf16FromLocal8Bit(error_code.message())
                  << " (" << error_code.value() << ")";
    onErrorOccurred(location, error);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    LOG(LS_ERROR) << "Connection finished with error " << errorToString(error_code)
                  << " from: " << location.toString();

    disconnect();
    emit sig_disconnected(error_code);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onResolved(
    const std::error_code &error_code, const asio::ip::tcp::resolver::results_type& endpoints)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    LOG(LS_INFO) << "Resolved endpoints: " << endpointsToString(endpoints);

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
        std::bind(&Handler::onConnected, handler_, std::placeholders::_1, std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onConnected(const std::error_code &error_code, const asio::ip::tcp::endpoint &endpoint)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    LOG(LS_INFO) << "Connected to endpoint: " << endpoint.address().to_string()
                 << ":" << endpoint.port();
    connected_ = true;
    emit sig_connected();
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onMessageWritten(quint8 channel_id)
{
    emit sig_messageWritten(channel_id, write_queue_.size());
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onMessageReceived()
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

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_size));

    if (!decryptor_->decrypt(read_data, read_size, decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
        return;
    }

    emit sig_messageReceived(header.channel_id, decrypt_buffer_);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::addWriteTask(WriteTask::Type type, quint8 channel_id, QByteArray&& data)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(type, channel_id, std::move(data));

    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::doWrite()
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
        // Calculate the size of the encrypted message.
        size_t target_data_size = encryptor_->encryptedDataSize(source_buffer.size());
        if (is_channel_id_supported_)
            target_data_size += sizeof(UserDataHeader);

        if (target_data_size > kMaxMessageSize)
        {
            LOG(LS_ERROR) << "Too big outgoing message: " << target_data_size;
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

        // Encrypt the message.
        if (!encryptor_->encrypt(source_buffer.data(), source_buffer.size(), write_buffer))
        {
            onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
            return;
        }
    }
    else
    {
        DCHECK_EQ(task.type(), WriteTask::Type::SERVICE_DATA);

        resizeBuffer(&write_buffer_, source_buffer.size());

        // Service data does not need encryption. Copy the source buffer.
        memcpy(write_buffer_.data(), source_buffer.data(), source_buffer.size());
    }

    // Send the buffer to the recipient.
    asio::async_write(socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      std::bind(&Handler::onWrite,
                                handler_,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK(!write_queue_.empty());

    // Update TX statistics.
    addTxBytes(bytes_transferred);

    const WriteTask& task = write_queue_.front();
    WriteTask::Type task_type = task.type();
    quint8 channel_id = task.channelId();

    // Delete the sent message from the queue.
    write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    bool schedule_write = !write_queue_.empty() || proxy_->reloadWriteQueue(&write_queue_);

    if (task_type == WriteTask::Type::USER_DATA)
        onMessageWritten(channel_id);

    if (schedule_write)
        doWrite();
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::doReadSize()
{
    state_ = ReadState::READ_SIZE;
    asio::async_read(socket_,
                     variable_size_reader_.buffer(),
                     std::bind(&Handler::onReadSize,
                               handler_,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onReadSize(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
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
            LOG(LS_ERROR) << "Too big incoming message: " << message_size;
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
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::doReadUserData(size_t length)
{
    resizeBuffer(&read_buffer_, length);

    state_ = ReadState::READ_USER_DATA;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&Handler::onReadUserData,
                               handler_,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onReadUserData(const std::error_code& error_code, size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_USER_DATA);

    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    // Update RX statistics.
    addRxBytes(bytes_transferred);

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

    doReadSize();
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::doReadServiceHeader()
{
    resizeBuffer(&read_buffer_, sizeof(ServiceHeader));

    state_ = ReadState::READ_SERVICE_HEADER;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&Handler::onReadServiceHeader,
                               handler_,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onReadServiceHeader(const std::error_code& error_code, size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    DCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));

    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK_EQ(bytes_transferred, read_buffer_.size());

    // Update RX statistics.
    addRxBytes(bytes_transferred);

    ServiceHeader* header = reinterpret_cast<ServiceHeader*>(read_buffer_.data());
    if (header->length > kMaxMessageSize)
    {
        LOG(LS_INFO) << "Too big service message: " << header->length;
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
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::doReadServiceData(size_t length)
{
    DCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
    DCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    DCHECK_GT(length, 0u);

    read_buffer_.resize(read_buffer_.size() + static_cast<QByteArray::size_type>(length));

    // Now we read the data after the header.
    state_ = ReadState::READ_SERVICE_DATA;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data() + sizeof(ServiceHeader),
                                  read_buffer_.size() - sizeof(ServiceHeader)),
                     std::bind(&Handler::onReadServiceData,
                               handler_,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onReadServiceData(const std::error_code& error_code, size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_SERVICE_DATA);
    DCHECK_GT(read_buffer_.size(), sizeof(ServiceHeader));

    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    // Update RX statistics.
    addRxBytes(bytes_transferred);

    // Incoming buffer contains a service header.
    ServiceHeader* header = reinterpret_cast<ServiceHeader*>(read_buffer_.data());

    DCHECK_EQ(bytes_transferred, read_buffer_.size() - sizeof(ServiceHeader));
    DCHECK_LE(header->length, kMaxMessageSize);

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
            if (read_buffer_.size() < (sizeof(ServiceHeader) + header->length))
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

            if (DCHECK_IS_ON())
            {
                Milliseconds ping_time = std::chrono::duration_cast<Milliseconds>(
                    Clock::now() - keep_alive_timestamp_);

                DLOG(LS_INFO) << "Ping result: " << ping_time.count() << " ms ("
                              << keep_alive_counter_.size() << " bytes)";
            }

            // The user can disable keep alive. Restart the timer only if keep alive is enabled.
            if (keep_alive_timer_)
            {
                DCHECK(!keep_alive_counter_.isEmpty());

                // Increase the counter of sent packets.
                largeNumberIncrement(&keep_alive_counter_);

                // Restart keep alive timer.
                keep_alive_timer_->cancel();
                keep_alive_timer_->expires_after(keep_alive_interval_);
                keep_alive_timer_->async_wait(
                    std::bind(&Handler::onKeepAliveInterval, handler_, std::placeholders::_1));
            }
        }
    }
    else
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    doReadSize();
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onKeepAliveInterval(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    DCHECK(keep_alive_timer_);

    if (error_code)
    {
        LOG(LS_ERROR) << "Keep alive timer error: " << utf16FromLocal8Bit(error_code.message());

        // Restarting the timer.
        keep_alive_timer_->cancel();
        keep_alive_timer_->expires_after(keep_alive_interval_);
        keep_alive_timer_->async_wait(
            std::bind(&Handler::onKeepAliveInterval, handler_, std::placeholders::_1));
    }
    else
    {
        // Save sending time.
        keep_alive_timestamp_ = Clock::now();

        // Send ping.
        sendKeepAlive(KEEP_ALIVE_PING, keep_alive_counter_.data(), keep_alive_counter_.size());

        // If a response is not received within the specified interval, the connection will be
        // terminated.
        keep_alive_timer_->cancel();
        keep_alive_timer_->expires_after(keep_alive_timeout_);
        keep_alive_timer_->async_wait(
            std::bind(&Handler::onKeepAliveTimeout, handler_, std::placeholders::_1));
    }
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::onKeepAliveTimeout(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    if (error_code)
    {
        LOG(LS_ERROR) << "Keep alive timer error: " << utf16FromLocal8Bit(error_code.message());
    }

    // No response came within the specified period of time. We forcibly terminate the connection.
    onErrorOccurred(FROM_HERE, ErrorCode::SOCKET_TIMEOUT);
}

//--------------------------------------------------------------------------------------------------
void TcpChannel::sendKeepAlive(quint8 flags, const void* data, size_t size)
{
    ServiceHeader header;
    memset(&header, 0, sizeof(header));

    header.type   = KEEP_ALIVE;
    header.flags  = flags;
    header.length = static_cast<quint32>(size);

    QByteArray buffer;
    buffer.resize(static_cast<QByteArray::size_type>(sizeof(quint8) + sizeof(header) + size));

    // The first byte set to 0 indicates that this is a service message.
    buffer[0] = 0;

    // Now copy the header and data to the buffer.
    memcpy(buffer.data() + sizeof(quint8), &header, sizeof(header));
    memcpy(buffer.data() + sizeof(quint8) + sizeof(header), data, size);

    // Add a task to the queue.
    addWriteTask(WriteTask::Type::SERVICE_DATA, 0, std::move(buffer));
}

} // namespace base
