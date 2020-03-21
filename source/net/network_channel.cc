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

#include "net/network_channel.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_asio.h"
#include "base/strings/unicode.h"
#include "crypto/message_encryptor_fake.h"
#include "crypto/message_decryptor_fake.h"
#include "net/network_channel_proxy.h"
#include "net/network_listener.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#if defined(OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

namespace net {

namespace {

static const size_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB

} // namespace

Channel::Channel()
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(io_context_),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context_)),
      proxy_(new ChannelProxy(base::MessageLoop::current()->taskRunner(), this)),
      encryptor_(std::make_unique<crypto::MessageEncryptorFake>()),
      decryptor_(std::make_unique<crypto::MessageDecryptorFake>())
{
    // Nothing
}

Channel::Channel(asio::ip::tcp::socket&& socket)
    : io_context_(base::MessageLoop::current()->pumpAsio()->ioContext()),
      socket_(std::move(socket)),
      proxy_(new ChannelProxy(base::MessageLoop::current()->taskRunner(), this)),
      encryptor_(std::make_unique<crypto::MessageEncryptorFake>()),
      decryptor_(std::make_unique<crypto::MessageDecryptorFake>()),
      connected_(true)
{
    DCHECK(socket_.is_open());
}

Channel::~Channel()
{
    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

    listener_ = nullptr;

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
    encryptor_ = std::move(encryptor);
}

void Channel::setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

std::u16string Channel::peerAddress() const
{
    if (!socket_.is_open())
        return std::u16string();

    return base::utf16FromLocal8Bit(socket_.remote_endpoint().address().to_string());
}

void Channel::connect(std::u16string_view address, uint16_t port)
{
    if (connected_ || !resolver_)
        return;

    resolver_->async_resolve(base::local8BitFromUtf16(address), std::to_string(port),
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

bool Channel::isConnected() const
{
    return connected_;
}

bool Channel::isPaused() const
{
    return paused_;
}

void Channel::pause()
{
    paused_ = true;
}

void Channel::resume()
{
    if (!connected_ || !paused_)
        return;

    paused_ = false;

    // We already have an incomplete read operation.
    if (state_ != ReadState::IDLE)
        return;

    // If we have a message that was received before the pause command.
    if (read_buffer_size_)
        onMessageReceived();

    DCHECK_EQ(bytes_transfered_, 0);
    DCHECK_EQ(read_buffer_size_, 0);

    doReadSize();
}

void Channel::send(base::ByteArray&& buffer)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(std::move(buffer));

    if (schedule_write)
        doWrite();
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

void Channel::disconnect()
{
    if (!connected_)
        return;

    connected_ = false;

    std::error_code ignored_code;

    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

void Channel::onErrorOccurred(const base::Location& location, const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    ErrorCode error = ErrorCode::UNKNOWN;

    if (error_code == asio::error::access_denied)
        error = ErrorCode::ACCESS_DENIED;
    else if (error_code == asio::error::host_not_found)
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
                    << " (code: " << error_code.value()
                    << ", location: " << location.toString() << ")";

    disconnect();

    if (listener_)
    {
        listener_->onDisconnected(error);
        listener_ = nullptr;
    }
}

void Channel::onMessageWritten()
{
    if (listener_)
        listener_->onMessageWritten();
}

void Channel::onMessageReceived()
{
    const size_t decrypt_buffer_size = decryptor_->decryptedDataSize(read_buffer_.size());

    if (decrypt_buffer_.capacity() < decrypt_buffer_size)
        decrypt_buffer_.reserve(decrypt_buffer_size);

    decrypt_buffer_.resize(decrypt_buffer_size);

    if (!decryptor_->decrypt(read_buffer_.data(), read_buffer_.size(), decrypt_buffer_.data()))
    {
        onErrorOccurred(FROM_HERE, asio::error::access_denied);
        return;
    }

    if (listener_)
        listener_->onMessageReceived(decrypt_buffer_);

    bytes_transfered_ = 0;
    read_buffer_size_ = 0;
}

void Channel::doWrite()
{
    const base::ByteArray& source_buffer = write_queue_.front();
    if (source_buffer.empty())
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    // Calculate the size of the encrypted message.
    const size_t target_data_size = encryptor_->encryptedDataSize(source_buffer.size());

    if (target_data_size > kMaxMessageSize)
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    uint8_t length_data[4];
    size_t length_data_size = 1;

    // Calculate the variable-length.
    length_data[0] = target_data_size & 0x7F;
    if (target_data_size > 0x7F) // 127 bytes
    {
        length_data[0] |= 0x80;
        length_data[length_data_size++] = target_data_size >> 7 & 0x7F;

        if (target_data_size > 0x3FFF) // 16383 bytes
        {
            length_data[1] |= 0x80;
            length_data[length_data_size++] = target_data_size >> 14 & 0x7F;

            if (target_data_size > 0x1FFFF) // 2097151 bytes
            {
                length_data[2] |= 0x80;
                length_data[length_data_size++] = target_data_size >> 21 & 0xFF;
            }
        }
    }

    // Now we can calculate the full size.
    const size_t total_size = length_data_size + target_data_size;

    // If the reserved buffer size is less, then increase it.
    if (write_buffer_.capacity() < total_size)
        write_buffer_.reserve(total_size);

    // Change the size of the buffer.
    write_buffer_.resize(total_size);

    // Copy the size of the message to the buffer.
    memcpy(write_buffer_.data(), length_data, length_data_size);

    // Encrypt the message.
    if (!encryptor_->encrypt(source_buffer.data(),
                             source_buffer.size(),
                             write_buffer_.data() + length_data_size))
    {
        onErrorOccurred(FROM_HERE, asio::error::access_denied);
        return;
    }

    // Send the buffer to the recipient.
    asio::async_write(socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      std::bind(&Channel::onWrite,
                                this,
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void Channel::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK(!write_queue_.empty());

    onMessageWritten();

    // Delete the sent message from the queue.
    write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    if (write_queue_.empty() && !proxy_->reloadWriteQueue(&write_queue_))
        return;

    doWrite();
}

void Channel::doReadSize()
{
    state_ = ReadState::READ_SIZE;

    asio::async_read(socket_,
                     asio::buffer(&one_byte_, 1),
                     std::bind(&Channel::onReadSize,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void Channel::onReadSize(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK_EQ(bytes_transferred, 1);

    bytes_transfered_ += bytes_transferred;

    switch (bytes_transfered_)
    {
        case 1:
            read_buffer_size_ += one_byte_ & 0x7F;
            break;

        case 2:
            read_buffer_size_ += (one_byte_ & 0x7F) << 7;
            break;

        case 3:
            read_buffer_size_ += (one_byte_ & 0x7F) << 14;
            break;

        case 4:
            read_buffer_size_ += one_byte_ << 21;
            break;

        default:
            break;
    }

    if ((one_byte_ & 0x80) && (bytes_transfered_ < 4))
    {
        doReadSize();
        return;
    }

    doReadContent();
}

void Channel::doReadContent()
{
    state_ = ReadState::READ_CONTENT;

    if (!read_buffer_size_ || read_buffer_size_ > kMaxMessageSize)
    {
        onErrorOccurred(FROM_HERE, asio::error::message_size);
        return;
    }

    if (read_buffer_.capacity() < read_buffer_size_)
        read_buffer_.reserve(read_buffer_size_);

    read_buffer_.resize(read_buffer_size_);

    asio::async_read(socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&Channel::onReadContent,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void Channel::onReadContent(const std::error_code& error_code, size_t bytes_transferred)
{
    if (error_code)
    {
        onErrorOccurred(FROM_HERE, error_code);
        return;
    }

    DCHECK_EQ(bytes_transferred, read_buffer_.size());

    if (paused_)
    {
        state_ = ReadState::IDLE;
        return;
    }

    onMessageReceived();

    if (paused_)
    {
        state_ = ReadState::IDLE;
        return;
    }

    DCHECK_EQ(bytes_transfered_, 0);
    DCHECK_EQ(read_buffer_size_, 0);

    doReadSize();
}

} // namespace net
