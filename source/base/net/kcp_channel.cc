//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/kcp_channel.h"

#include "base/logging.h"
#include "base/crypto/large_number_increment.h"
#include "base/crypto/message_decryptor.h"
#include "base/crypto/message_encryptor.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/net/kcp_channel_proxy.h"
#include "base/strings/unicode.h"

#include <asio/connect.hpp>

#if defined(USE_MIMALLOC)
#include <mimalloc.h>
#endif // defined(USE_MIMALLOC)

namespace base {

KcpChannel::KcpChannel()
    : update_timer_(std::make_unique<asio::high_resolution_timer>(
          MessageLoop::current()->pumpAsio()->ioContext())),
      proxy_(new KcpChannelProxy(MessageLoop::current()->taskRunner(), this)),
      socket_(MessageLoop::current()->pumpAsio()->ioContext())
{
    LOG(LS_INFO) << "Ctor";
    initKcp();
}

KcpChannel::~KcpChannel()
{
    LOG(LS_INFO) << "Dtor";

    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    listener_ = nullptr;
    disconnect();

    if (kcp_)
    {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

std::shared_ptr<KcpChannelProxy> KcpChannel::channelProxy()
{
    return proxy_;
}

void KcpChannel::setListener(Listener* listener)
{
    listener_ = listener;
}

void KcpChannel::setEncryptor(std::unique_ptr<MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
}

void KcpChannel::setDecryptor(std::unique_ptr<MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

void KcpChannel::connect(std::u16string_view host, uint16_t port)
{
    if (!kcp_)
    {
        LOG(LS_WARNING) << "KCP not initialized";
        return;
    }

    if (connected_)
    {
        LOG(LS_WARNING) << "Already connected";
        return;
    }

    resolver_ = std::make_unique<asio::ip::udp::resolver>(
        MessageLoop::current()->pumpAsio()->ioContext());

    resolver_->async_resolve(local8BitFromUtf16(host), std::to_string(port),
        [=](const std::error_code& error_code,
            const asio::ip::udp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        asio::async_connect(socket_, endpoints,
            [=](const std::error_code& error_code,
                   const asio::ip::udp::endpoint& endpoint)
        {
            if (error_code)
            {
                onErrorOccurred(FROM_HERE, error_code);
                return;
            }

            LOG(LS_INFO) << "Connected to " << endpoint.address() << ":" << endpoint.port();
            connected_ = true;

            if (listener_)
                listener_->onKcpConnected();
        });
    });
}

bool KcpChannel::isConnected() const
{
    return connected_;
}

bool KcpChannel::isPaused() const
{
    return paused_;
}

void KcpChannel::pause()
{
    paused_ = true;
    stopUpdateTimer();
}

void KcpChannel::resume()
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

    startUpdateTimer(15);
}

void KcpChannel::send(uint8_t channel_id, ByteArray&& buffer)
{
    addWriteTask(WriteTask::Type::USER_DATA, channel_id, std::move(buffer));
}

bool KcpChannel::setKeepAlive(bool enable, const Seconds& interval, const Seconds& timeout)
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

        keep_alive_timer_ = std::make_unique<asio::high_resolution_timer>(
            MessageLoop::current()->pumpAsio()->ioContext());
        keep_alive_timer_->expires_after(keep_alive_interval_);
        keep_alive_timer_->async_wait(
            std::bind(&KcpChannel::onKeepAliveInterval, this, std::placeholders::_1));
    }

    return true;
}

size_t KcpChannel::pendingMessages() const
{
    return write_queue_.size();
}

void KcpChannel::disconnect()
{
    if (!connected_)
        return;

    connected_ = false;

    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);

    stopUpdateTimer();
}

void KcpChannel::onErrorOccurred(const Location& location, const std::error_code& error_code)
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

void KcpChannel::onErrorOccurred(const Location& location, ErrorCode error_code)
{
    LOG(LS_WARNING) << "Connection finished with error " << errorToString(error_code)
                    << " from: " << location.toString();

    disconnect();

    if (listener_)
    {
        listener_->onKcpDisconnected(error_code);
        listener_ = nullptr;
    }
}

void KcpChannel::onMessageReceived()
{
    uint8_t channel_id = read_buffer_[0];
    uint8_t* read_data = read_buffer_.data() + sizeof(channel_id);
    size_t read_size = read_buffer_.size() - sizeof(channel_id);

    resizeBuffer(&decrypt_buffer_, decryptor_->decryptedDataSize(read_size));

    if (!decryptor_->decrypt(read_data, read_size, decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::ACCESS_DENIED);
        return;
    }

    if (listener_)
        listener_->onKcpMessageReceived(channel_id, decrypt_buffer_);
}

void KcpChannel::addWriteTask(WriteTask::Type type, uint8_t channel_id, ByteArray&& data)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(type, channel_id, std::move(data));

    if (schedule_write)
        doWrite();
}

void KcpChannel::doWrite()
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
        const uint8_t channel_id = task.channelId();

        // Calculate the size of the encrypted message.
        const uint32_t target_data_size = encryptor_->encryptedDataSize(source_buffer.size()) +
            sizeof(channel_id);

        if (target_data_size > kMaxMessageSize)
        {
            LOG(LS_ERROR) << "Too big outgoing message: " << target_data_size;
            onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
            return;
        }

        resizeBuffer(&write_buffer_, sizeof(target_data_size) + target_data_size);

        // Copy the size of the message to the buffer.
        memcpy(write_buffer_.data(), &target_data_size, sizeof(target_data_size));

        // Copy the channel id to the buffer.
        memcpy(write_buffer_.data() + sizeof(target_data_size), &channel_id, sizeof(channel_id));

        // Encrypt the message.
        if (!encryptor_->encrypt(source_buffer.data(),
                                 source_buffer.size(),
                                 write_buffer_.data() + sizeof(target_data_size) + sizeof(channel_id)))
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

    int ret = ikcp_send(kcp_,
                        reinterpret_cast<const char*>(write_buffer_.data()),
                        static_cast<int>(write_buffer_.size()));
    if (ret < 0)
    {
        LOG(LS_WARNING) << "ikcp_send failed: " << ret;
        onErrorOccurred(FROM_HERE, ErrorCode::NETWORK_ERROR);
    }
}

void KcpChannel::doReadSize()
{
    state_ = ReadState::READ_SIZE;
    recv_buffer_pos_ = 0;

    doRead(asio::buffer(&read_size_, sizeof(read_size_)),
           std::bind(&KcpChannel::onReadSize, this, std::placeholders::_1));
}

void KcpChannel::onReadSize(size_t bytes_transferred)
{
    if (bytes_transferred != sizeof(read_size_))
    {
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    if (read_size_ > kMaxMessageSize)
    {
        LOG(LS_ERROR) << "Too big incoming message: " << read_size_;
        onErrorOccurred(FROM_HERE, ErrorCode::INVALID_PROTOCOL);
        return;
    }

    // If the message size is 0 (in other words, the first received byte is 0), then you need
    // to start reading the service message.
    if (!read_size_)
    {
        doReadServiceHeader();
        return;
    }

    doReadUserData(read_size_);
}

void KcpChannel::doReadUserData(size_t length)
{
    resizeBuffer(&read_buffer_, length);

    state_ = ReadState::READ_USER_DATA;
    recv_buffer_pos_ = 0;

    doRead(asio::buffer(read_buffer_.data(), read_buffer_.size()),
           std::bind(&KcpChannel::onReadUserData, this, std::placeholders::_1));
}

void KcpChannel::onReadUserData(size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_USER_DATA);
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

void KcpChannel::doReadServiceHeader()
{
    resizeBuffer(&read_buffer_, sizeof(ServiceHeader));

    state_ = ReadState::READ_SERVICE_HEADER;
    recv_buffer_pos_ = 0;

    doRead(asio::buffer(read_buffer_.data(), read_buffer_.size()),
           std::bind(&KcpChannel::onReadServiceHeader, this, std::placeholders::_1));
}

void KcpChannel::onReadServiceHeader(size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    DCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
    DCHECK_EQ(bytes_transferred, read_buffer_.size());

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

void KcpChannel::doReadServiceData(size_t length)
{
    DCHECK_EQ(read_buffer_.size(), sizeof(ServiceHeader));
    DCHECK_EQ(state_, ReadState::READ_SERVICE_HEADER);
    DCHECK_GT(length, 0u);

    read_buffer_.resize(read_buffer_.size() + length);

    // Now we read the data after the header.
    state_ = ReadState::READ_SERVICE_DATA;
    recv_buffer_pos_ = 0;

    doRead(asio::buffer(read_buffer_.data() + sizeof(ServiceHeader),
                        read_buffer_.size() - sizeof(ServiceHeader)),
           std::bind(&KcpChannel::onReadServiceData, this, std::placeholders::_1));
}

void KcpChannel::onReadServiceData(size_t bytes_transferred)
{
    DCHECK_EQ(state_, ReadState::READ_SERVICE_DATA);
    DCHECK_GT(read_buffer_.size(), sizeof(ServiceHeader));

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
                    std::bind(&KcpChannel::onKeepAliveInterval, this, std::placeholders::_1));
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

void KcpChannel::doRead(asio::mutable_buffer buffer, ReadCompleteCallback callback)
{
    if (!connected_)
    {
        onErrorOccurred(FROM_HERE, ErrorCode::NETWORK_ERROR);
        return;
    }

    socket_.async_receive(
        asio::buffer(input_buffer_.data(), input_buffer_.size()),
        [=](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        // Update RX statistics.
        addRxBytes(bytes_transferred);

        int ret = ikcp_input(kcp_, input_buffer_.data(), static_cast<long>(bytes_transferred));
        if (ret < 0)
        {
            LOG(LS_WARNING) << "ikcp_input failed: " << ret;
            onErrorOccurred(FROM_HERE, ErrorCode::NETWORK_ERROR);
        }
        else
        {
            int len = ikcp_recv(kcp_,
                                reinterpret_cast<char*>(recv_buffer_.data()) + recv_buffer_pos_,
                                static_cast<int>(recv_buffer_.size() - recv_buffer_pos_));
            if (len > 0)
            {
                recv_buffer_pos_ += static_cast<size_t>(len);

                if (recv_buffer_pos_ == recv_buffer_.size())
                {
                    // Message received in full.
                    callback(buffer.size());
                }
                else if (recv_buffer_pos_ < buffer.size())
                {
                    // Message received incomplete.
                    doRead(buffer, std::move(callback));
                }
                else
                {
                    LOG(LS_FATAL) << "Invalid position for read buffer";
                    return;
                }
            }
            else if (len == 0)
            {
                // No data yet.
                doRead(buffer, std::move(callback));
            }
            else
            {
                onErrorOccurred(FROM_HERE, ErrorCode::NETWORK_ERROR);
            }
        }
    });
}

void KcpChannel::startUpdateTimer(uint32_t timeout)
{
    update_timer_->expires_after(Milliseconds(timeout));
    update_timer_->async_wait(
        std::bind(&KcpChannel::onUpdateTimeout, this, std::placeholders::_1));
}

void KcpChannel::stopUpdateTimer()
{
    update_timer_->cancel();
}

void KcpChannel::onUpdateTimeout(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    DCHECK(update_timer_);

    if (error_code)
    {
        LOG(LS_ERROR) << "Keep alive timer error: " << utf16FromLocal8Bit(error_code.message());
    }
    else
    {
        TimePoint current_time = Clock::now();
        uint32_t time = static_cast<uint32_t>(
            std::chrono::duration_cast<Milliseconds>(current_time - last_service_time_).count());
        last_service_time_ = current_time;

        onUpdate(time);

        startUpdateTimer(ikcp_check(kcp_, time));
    }
}

void KcpChannel::onUpdate(uint32_t time)
{
    ikcp_update(kcp_, time);
}

void KcpChannel::initKcp()
{
#if defined(USE_MIMALLOC)
    ikcp_allocator(mi_new, mi_free);
#endif // defined(USE_MIMALLOC)

    kcp_ = ikcp_create(0xFA01BB4E, this);
    CHECK(kcp_) << "ikcp_create failed";

    ikcp_setoutput(kcp_, onDataWriteCallback);

    int ret = ikcp_nodelay(kcp_, 1, 20, 2, 1);
    if (ret != 0)
    {
        LOG(LS_WARNING) << "ikcp_nodelay failed: " << ret;
    }

    ret = ikcp_setmtu(kcp_, 1200);
    if (ret != 0)
    {
        LOG(LS_WARNING) << "ikcp_setmtu failed: " << ret;
    }

    ret = ikcp_wndsize(kcp_, 1024, 1024);
    if (ret != 0)
    {
        LOG(LS_WARNING) << "ikcp_wndsize failed: " << ret;
    }
}

void KcpChannel::onKeepAliveInterval(const std::error_code& error_code)
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
            std::bind(&KcpChannel::onKeepAliveInterval, this, std::placeholders::_1));
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
            std::bind(&KcpChannel::onKeepAliveTimeout, this, std::placeholders::_1));
    }
}

void KcpChannel::onKeepAliveTimeout(const std::error_code& error_code)
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

void KcpChannel::sendKeepAlive(uint8_t flags, const void* data, size_t size)
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
    addWriteTask(WriteTask::Type::SERVICE_DATA, 0, std::move(buffer));
}

void KcpChannel::onDataWrite(const char* buf, size_t len)
{
    socket_.async_send(asio::buffer(buf, len),
                       [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        // Update TX statistics.
        addTxBytes(bytes_transferred);

        // Check that all data from the KCP has been completely sent.
        if (ikcp_waitsnd(kcp_) == 0)
        {
            DCHECK(!write_queue_.empty());

            const WriteTask& task = write_queue_.front();
            WriteTask::Type task_type = task.type();
            uint8_t channel_id = task.channelId();

            // Delete the sent message from the queue.
            write_queue_.pop();

            // If the queue is not empty, then we send the following message.
            bool schedule_write = !write_queue_.empty() || proxy_->reloadWriteQueue(&write_queue_);

            if (task_type == WriteTask::Type::USER_DATA)
                onMessageWritten(channel_id);

            if (schedule_write)
                doWrite();
        }
    });
}

// static
int KcpChannel::onDataWriteCallback(const char* buf, int len, struct IKCPCB* /* kcp */, void* user)
{
    KcpChannel* self = reinterpret_cast<KcpChannel*>(user);
    DCHECK(self);

    self->onDataWrite(buf, static_cast<size_t>(len));
    return 0;
}

} // namespace base
