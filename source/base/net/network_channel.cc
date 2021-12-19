//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/network_channel.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_encryptor_fake.h"
#include "base/crypto/message_decryptor_fake.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/net/network_channel_proxy.h"
#include "base/net/tcp_keep_alive.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

namespace base {

namespace {

static const size_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB

int calculateSpeed(int last_speed, const std::chrono::milliseconds& duration, int64_t bytes)
{
    static const double kAlpha = 0.1;
    return static_cast<int>(
        (kAlpha * ((1000.0 / static_cast<double>(duration.count())) * static_cast<double>(bytes))) +
        ((1.0 - kAlpha) * static_cast<double>(last_speed)));
}

void resizeBuffer(ByteArray* buffer, size_t new_size)
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

NetworkChannel::NetworkChannel()
    : proxy_(new NetworkChannelProxy(MessageLoop::current()->taskRunner(), this)),
      io_context_(MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      encryptor_(std::make_unique<MessageEncryptorFake>()),
      decryptor_(std::make_unique<MessageDecryptorFake>())
{
    LOG(LS_INFO) << "Ctor";
}

NetworkChannel::NetworkChannel(asio::ip::tcp::socket&& socket)
    : proxy_(new NetworkChannelProxy(MessageLoop::current()->taskRunner(), this)),
      io_context_(MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(std::move(socket)),
      connected_(true),
      encryptor_(std::make_unique<MessageEncryptorFake>()),
      decryptor_(std::make_unique<MessageDecryptorFake>())
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(socket_.is_open());
}

NetworkChannel::~NetworkChannel()
{
    LOG(LS_INFO) << "Dtor";

    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    listener_ = nullptr;
    disconnect();
}

std::shared_ptr<NetworkChannelProxy> NetworkChannel::channelProxy()
{
    return proxy_;
}

void NetworkChannel::setListener(Listener* listener)
{
    listener_ = listener;
}

void NetworkChannel::setEncryptor(std::unique_ptr<MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
}

void NetworkChannel::setDecryptor(std::unique_ptr<MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

std::u16string NetworkChannel::peerAddress() const
{
    if (!socket_.is_open())
        return std::u16string();

    return utf16FromLocal8Bit(socket_.remote_endpoint().address().to_string());
}

void NetworkChannel::connect(std::u16string_view address, uint16_t port)
{
    if (connected_ || !resolver_)
        return;

    resolver_->async_resolve(local8BitFromUtf16(address), std::to_string(port),
        [this](const std::error_code& error_code,
               const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        asio::async_connect(socket_, endpoints,
            [this](const std::error_code& error_code,
                   const asio::ip::tcp::endpoint& /* endpoint */)
        {
            if (error_code)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            connected_ = true;

            if (listener_)
                listener_->onConnected();
        });
    });
}

bool NetworkChannel::isConnected() const
{
    return connected_;
}

bool NetworkChannel::isPaused() const
{
    return paused_;
}

void NetworkChannel::pause()
{
    paused_ = true;
}

void NetworkChannel::resume()
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

void NetworkChannel::send(ByteArray&& buffer)
{
    addWriteTask(WriteTask::Type::USER_DATA, std::move(buffer));
}

bool NetworkChannel::setNoDelay(bool enable)
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

bool NetworkChannel::setTcpKeepAlive(bool enable,
                                     const Milliseconds& time,
                                     const Milliseconds& interval)
{
    return base::setTcpKeepAlive(socket_.native_handle(), enable, time, interval);
}

bool NetworkChannel::setOwnKeepAlive(bool enable, const Seconds& interval, const Seconds& timeout)
{
    if (enable && keep_alive_timer_)
    {
        LOG(LS_WARNING) << "Keep alive already active";
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
        keep_alive_counter_.clear();

        if (keep_alive_timer_)
        {
            keep_alive_timer_->cancel();
            keep_alive_timer_.reset();
        }
    }
    else
    {
        keep_alive_interval_ = interval;
        keep_alive_timeout_ = timeout;

        keep_alive_counter_.resize(sizeof(uint32_t));
        memset(keep_alive_counter_.data(), 0, keep_alive_counter_.size());

        keep_alive_timer_ = std::make_unique<asio::high_resolution_timer>(io_context_);
        keep_alive_timer_->expires_after(keep_alive_interval_);
        keep_alive_timer_->async_wait(
            std::bind(&NetworkChannel::onKeepAliveInterval, this, std::placeholders::_1));
    }

    return true;
}

bool NetworkChannel::setReadBufferSize(size_t size)
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

bool NetworkChannel::setWriteBufferSize(size_t size)
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

int NetworkChannel::speedRx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_rx_);

    speed_rx_ = calculateSpeed(speed_rx_, duration, bytes_rx_);

    begin_time_rx_ = current_time;
    bytes_rx_ = 0;

    return speed_rx_;
}

int NetworkChannel::speedTx()
{
    TimePoint current_time = Clock::now();
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(current_time - begin_time_tx_);

    speed_tx_ = calculateSpeed(speed_tx_, duration, bytes_tx_);

    begin_time_tx_ = current_time;
    bytes_tx_ = 0;

    return speed_tx_;
}

// static
std::string NetworkChannel::errorToString(ErrorCode error_code)
{
    const char* str;

    switch (error_code)
    {
        case ErrorCode::SUCCESS:
            str = "SUCCESS";
            break;

        case ErrorCode::INVALID_PROTOCOL:
            str = "INVALID_PROTOCOL";
            break;

        case ErrorCode::ACCESS_DENIED:
            str = "ACCESS_DENIED";
            break;

        case ErrorCode::NETWORK_ERROR:
            str = "NETWORK_ERROR";
            break;

        case ErrorCode::CONNECTION_REFUSED:
            str = "CONNECTION_REFUSED";
            break;

        case ErrorCode::REMOTE_HOST_CLOSED:
            str = "REMOTE_HOST_CLOSED";
            break;

        case ErrorCode::SPECIFIED_HOST_NOT_FOUND:
            str = "SPECIFIED_HOST_NOT_FOUND";
            break;

        case ErrorCode::SOCKET_TIMEOUT:
            str = "SOCKET_TIMEOUT";
            break;

        case ErrorCode::ADDRESS_IN_USE:
            str = "ADDRESS_IN_USE";
            break;

        case ErrorCode::ADDRESS_NOT_AVAILABLE:
            str = "ADDRESS_NOT_AVAILABLE";
            break;

        default:
            str = "UNKNOWN";
            break;
    }

    return stringPrintf("%s (%d)", str, static_cast<int>(error_code));
}

void NetworkChannel::disconnect()
{
    if (!connected_)
        return;

    connected_ = false;

    std::error_code ignored_code;

    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

void NetworkChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
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

    LOG(LS_WARNING) << "Asio error: " << utf16FromLocal8Bit(error_code.message())
                    << " (" << error_code.value() << ")";
    onErrorOccurred(location, error);
}

void NetworkChannel::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    LOG(LS_WARNING) << "Connection finished with error " << errorToString(error_code)
                    << " from: " << location.toString();

    disconnect();

    if (listener_)
    {
        listener_->onDisconnected(error_code);
        listener_ = nullptr;
    }
}

void NetworkChannel::onMessageWritten()
{
    if (listener_)
        listener_->onMessageWritten(write_queue_.size());
}

void NetworkChannel::onMessageReceived()
{
    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_buffer_.size()));

    if (!decryptor_->decrypt(read_buffer_.data(), read_buffer_.size(), decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
        return;
    }

    if (listener_)
        listener_->onMessageReceived(decrypt_buffer_);
}

void NetworkChannel::addWriteTask(WriteTask::Type type, ByteArray&& data)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(type, std::move(data));

    if (schedule_write)
        doWrite();
}

void NetworkChannel::doWrite()
{
    const WriteTask& task = write_queue_.front();
    const ByteArray& source_buffer = task.data();

    if (source_buffer.empty())
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (task.type() == WriteTask::Type::USER_DATA)
    {
        // Calculate the size of the encrypted message.
        const size_t target_data_size = encryptor_->encryptedDataSize(source_buffer.size());

        if (target_data_size > kMaxMessageSize)
        {
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        asio::const_buffer variable_size = variable_size_writer_.variableSize(target_data_size);

        resizeBuffer(&write_buffer_, variable_size.size() + target_data_size);

        // Copy the size of the message to the buffer.
        memcpy(write_buffer_.data(), variable_size.data(), variable_size.size());

        // Encrypt the message.
        if (!encryptor_->encrypt(source_buffer.data(),
                                 source_buffer.size(),
                                 write_buffer_.data() + variable_size.size()))
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
                      std::bind(&NetworkChannel::onWrite,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void NetworkChannel::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK(!write_queue_.empty());

    // Update TX statistics.
    addTxBytes(bytes_transferred);

    WriteTask::Type task_type = write_queue_.front().type();

    // Delete the sent message from the queue.
    write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    bool schedule_write = !write_queue_.empty() || proxy_->reloadWriteQueue(&write_queue_);

    if (task_type == WriteTask::Type::USER_DATA)
        onMessageWritten();

    if (schedule_write)
        doWrite();
}

void NetworkChannel::doReadSize()
{
    state_ = ReadState::READ_SIZE;
    asio::async_read(socket_,
                     variable_size_reader_.buffer(),
                     std::bind(&NetworkChannel::onReadSize,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannel::onReadSize(const std::error_code& error_code, size_t bytes_transferred)
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

void NetworkChannel::doReadUserData(size_t length)
{
    resizeBuffer(&read_buffer_, length);

    state_ = ReadState::READ_USER_DATA;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&NetworkChannel::onReadUserData,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannel::onReadUserData(const std::error_code& error_code, size_t bytes_transferred)
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

void NetworkChannel::doReadServiceHeader()
{
    resizeBuffer(&read_buffer_, sizeof(ServiceHeader));

    state_ = ReadState::READ_SERVICE_HEADER;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&NetworkChannel::onReadServiceHeader,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannel::onReadServiceHeader(const std::error_code& error_code, size_t bytes_transferred)
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

void NetworkChannel::doReadServiceData(size_t length)
{
    DCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
    DCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    DCHECK_GT(length, 0u);

    read_buffer_.resize(read_buffer_.size() + length);

    // Now we read the data after the header.
    state_ = ReadState::READ_SERVICE_DATA;
    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data() + sizeof(ServiceHeader),
                                  read_buffer_.size() - sizeof(ServiceHeader)),
                     std::bind(&NetworkChannel::onReadServiceData,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void NetworkChannel::onReadServiceData(const std::error_code& error_code, size_t bytes_transferred)
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
                DCHECK(!keep_alive_counter_.empty());

                // Increase the counter of sent packets.
                largeNumberIncrement(&keep_alive_counter_);

                // Restart keep alive timer.
                keep_alive_timer_->expires_after(keep_alive_interval_);
                keep_alive_timer_->async_wait(
                    std::bind(&NetworkChannel::onKeepAliveInterval, this, std::placeholders::_1));
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

void NetworkChannel::onKeepAliveInterval(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    DCHECK(keep_alive_timer_);

    if (error_code)
    {
        LOG(LS_ERROR) << "Keep alive timer error: " << utf16FromLocal8Bit(error_code.message());

        // Restarting the timer.
        keep_alive_timer_->expires_after(keep_alive_interval_);
        keep_alive_timer_->async_wait(
            std::bind(&NetworkChannel::onKeepAliveInterval, this, std::placeholders::_1));
    }
    else
    {
        // Save sending time.
        keep_alive_timestamp_ = Clock::now();

        // Send ping.
        sendKeepAlive(KEEP_ALIVE_PING, keep_alive_counter_.data(), keep_alive_counter_.size());

        // If a response is not received within the specified interval, the connection will be
        // terminated.
        keep_alive_timer_->expires_after(keep_alive_timeout_);
        keep_alive_timer_->async_wait(
            std::bind(&NetworkChannel::onKeepAliveTimeout, this, std::placeholders::_1));
    }
}

void NetworkChannel::onKeepAliveTimeout(const std::error_code& error_code)
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

void NetworkChannel::sendKeepAlive(uint8_t flags, const void* data, size_t size)
{
    ServiceHeader header;
    memset(&header, 0, sizeof(header));

    header.type   = KEEP_ALIVE;
    header.flags  = flags;
    header.length = static_cast<uint32_t>(size);

    ByteArray buffer;
    buffer.resize(sizeof(uint8_t) + sizeof(header) + size);

    // The first byte set to 0 indicates that this is a service message.
    buffer[0] = 0;

    // Now copy the header and data to the buffer.
    memcpy(buffer.data() + sizeof(uint8_t), &header, sizeof(header));
    memcpy(buffer.data() + sizeof(uint8_t) + sizeof(header), data, size);

    // Add a task to the queue.
    addWriteTask(WriteTask::Type::SERVICE_DATA, std::move(buffer));
}

void NetworkChannel::addTxBytes(size_t bytes_count)
{
    bytes_tx_ += bytes_count;
    total_tx_ += bytes_count;
}

void NetworkChannel::addRxBytes(size_t bytes_count)
{
    bytes_rx_ += bytes_count;
    total_rx_ += bytes_count;
}

} // namespace base
