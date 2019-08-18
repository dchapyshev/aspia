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
#include "base/strings/unicode.h"
#include "crypto/cryptor_fake.h"
#include "net/network_channel_proxy.h"
#include "net/network_listener.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <functional>

#if defined(OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

namespace net {

namespace {

const uint32_t kMaxMessageSize = 16 * 1024 * 1024; // 16 MB

void enableKeepAlive(asio::ip::tcp::socket& socket)
{
#if defined(OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = 1; // On.
    alive.keepalivetime = 60000; // 60 seconds.
    alive.keepaliveinterval = 5000; // 5 seconds.

    DWORD bytes_returned;

    if (WSAIoctl(socket.native_handle(), SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        PLOG(LS_WARNING) << "WSAIoctl failed";
    }
#else
#warning Not implemented
#endif
}

void disableNagle(asio::ip::tcp::socket& socket)
{
    // Disable the algorithm of Nagle.
    asio::ip::tcp::no_delay option(true);

    asio::error_code error_code;
    socket.set_option(option, error_code);

    if (error_code)
    {
        LOG(LS_ERROR) << "Failed to disable Nagle's algorithm: " << error_code.message();
    }
}

} // namespace

Channel::Channel(asio::io_context& io_context)
    : io_context_(io_context),
      resolver_(std::make_unique<asio::ip::tcp::resolver>(io_context)),
      socket_(io_context),
      proxy_(new ChannelProxy(this))
{
    // Nothing
}

Channel::Channel(asio::io_context& io_context, asio::ip::tcp::socket&& socket)
    : io_context_(io_context),
      socket_(std::move(socket)),
      proxy_(new ChannelProxy(this)),
      is_connected_(true)
{
    DCHECK(socket_.is_open());
    disableNagle(socket_);
}

Channel::~Channel()
{
    proxy_->willDestroyCurrentChannel();
    proxy_ = nullptr;

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

void Channel::setCryptor(std::unique_ptr<crypto::Cryptor> cryptor)
{
    cryptor_ = std::move(cryptor);
}

std::u16string Channel::peerAddress() const
{
    if (!socket_.is_open())
        return std::u16string();

    return base::utf16FromLocal8Bit(socket_.remote_endpoint().address().to_string());
}

void Channel::connect(std::u16string_view address, uint16_t port)
{
    DCHECK(resolver_);

    if (is_connected_)
        return;

    resolver_->async_resolve(base::local8BitFromUtf16(address), std::to_string(port), [this](
        const std::error_code& error_code, const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        asio::async_connect(socket_, endpoints, [this](
            const std::error_code& error_code, const asio::ip::tcp::endpoint& endpoint)
        {
            if (error_code)
            {
                onErrorOccurred(error_code);
                return;
            }

            disableNagle(socket_);
            enableKeepAlive(socket_);

            cryptor_ = std::make_unique<crypto::CryptorFake>();
            is_connected_ = true;

            if (listener_)
                listener_->onNetworkConnected();
        });
    });
}

void Channel::disconnect()
{
    if (!is_connected_)
        return;

    is_connected_ = false;

    std::error_code ignored_code;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_code);
    socket_.close(ignored_code);

    reloadWriteQueue();

    // Remove all messages from the write queue.
    while (!write_.work_queue.empty())
        write_.work_queue.pop();

    if (listener_)
    {
        listener_->onNetworkDisconnected();
        listener_ = nullptr;
    }
}

bool Channel::isConnected() const
{
    return is_connected_;
}

bool Channel::isPaused() const
{
    return is_paused_;
}

void Channel::pause()
{
    is_paused_ = true;
}

void Channel::resume()
{
    if (!is_connected_ || !is_paused_)
        return;

    is_paused_ = false;

    // If we have a message that was received before the pause command.
    if (read_.buffer_size)
        onMessageReceived();

    DCHECK_EQ(read_.bytes_transfered, 0);
    DCHECK_EQ(read_.buffer_size, 0);

    doReadSize();
}

void Channel::send(base::ByteArray&& buffer)
{
    std::scoped_lock lock(write_.incoming_queue_lock);

    const bool schedule_write = write_.incoming_queue.empty();

    // Add the buffer to the queue for sending.
    write_.incoming_queue.emplace(std::move(buffer));

    if (!schedule_write)
        return;

    asio::post(io_context_, std::bind(&Channel::scheduleWrite, this));
}

void Channel::onErrorOccurred(const std::error_code& error_code)
{
    if (error_code == asio::error::operation_aborted)
        return;

    ErrorCode error = ErrorCode::UNKNOWN;

    if (error_code == asio::error::connection_refused)
        error = ErrorCode::CONNECTION_REFUSED;
    else if (error_code == asio::error::address_in_use)
        error = ErrorCode::ADDRESS_IN_USE;
    else if (error_code == asio::error::timed_out)
        error = ErrorCode::SOCKET_TIMEOUT;
    else if (error_code == asio::error::host_unreachable)
        error = ErrorCode::ADDRESS_NOT_AVAILABLE;
    else if (error_code == asio::error::connection_reset)
        error = ErrorCode::REMOTE_HOST_CLOSED;
    else if (error_code == asio::error::network_down)
        error = ErrorCode::NETWORK_ERROR;

    if (listener_)
        listener_->onNetworkError(error);

    disconnect();
}

void Channel::doReadSize()
{
    asio::async_read(socket_, asio::buffer(&read_.one_byte, 1), [this](
        const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, 1);

        read_.bytes_transfered += bytes_transferred;

        switch (read_.bytes_transfered)
        {
            case 1:
                read_.buffer_size += read_.one_byte & 0x7F;
                break;

            case 2:
                read_.buffer_size += (read_.one_byte & 0x7F) << 7;
                break;

            case 3:
                read_.buffer_size += (read_.one_byte & 0x7F) << 14;
                break;

            case 4:
                read_.buffer_size += read_.one_byte << 21;
                break;

            default:
                break;
        }

        if ((read_.one_byte & 0x80) && (read_.bytes_transfered < 4))
        {
            doReadSize();
            return;
        }

        doReadContent();
    });
}

void Channel::doReadContent()
{
    if (!read_.buffer_size || read_.buffer_size > kMaxMessageSize)
    {
        onErrorOccurred(asio::error::message_size);
        return;
    }

    if (read_.buffer.capacity() < read_.buffer_size)
        read_.buffer.reserve(read_.buffer_size);

    read_.buffer.resize(read_.buffer_size);

    asio::async_read(socket_, asio::buffer(read_.buffer.data(), read_.buffer.size()), [this](
        const std::error_code & error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        DCHECK_EQ(bytes_transferred, read_.buffer.size());

        if (is_paused_)
            return;

        onMessageReceived();

        if (is_paused_)
            return;

        DCHECK_EQ(read_.bytes_transfered, 0);
        DCHECK_EQ(read_.buffer_size, 0);

        doReadSize();
    });
}

void Channel::onMessageReceived()
{
    base::ByteArray output_buffer;
    output_buffer.resize(cryptor_->decryptedDataSize(read_.buffer.size()));

    if (!cryptor_->decrypt(read_.buffer.data(),
                           read_.buffer.size(),
                           output_buffer.data()))
    {
        onErrorOccurred(asio::error::access_denied);
        return;
    }

    if (listener_)
        listener_->onNetworkMessage(output_buffer);

    read_.bytes_transfered = 0;
    read_.buffer_size = 0;
}

bool Channel::reloadWriteQueue()
{
    if (!write_.work_queue.empty())
        return false;

    std::scoped_lock lock(write_.incoming_queue_lock);

    if (write_.incoming_queue.empty())
        return false;

    write_.incoming_queue.fastSwap(write_.work_queue);
    DCHECK(write_.incoming_queue.empty());

    return true;
}

void Channel::scheduleWrite()
{
    if (!reloadWriteQueue())
        return;

    doWrite();
}

void Channel::doWrite()
{
    const base::ByteArray& source_buffer = write_.work_queue.front();
    if (source_buffer.empty())
    {
        onErrorOccurred(asio::error::message_size);
        return;
    }

    // Calculate the size of the encrypted message.
    const size_t target_data_size = cryptor_->encryptedDataSize(source_buffer.size());

    if (target_data_size > kMaxMessageSize)
    {
        onErrorOccurred(asio::error::message_size);
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
    if (write_.buffer.capacity() < total_size)
        write_.buffer.reserve(total_size);

    // Change the size of the buffer.
    write_.buffer.resize(total_size);

    // Copy the size of the message to the buffer.
    memcpy(write_.buffer.data(), length_data, length_data_size);

    // Encrypt the message.
    if (!cryptor_->encrypt(source_buffer.data(),
                           source_buffer.size(),
                           write_.buffer.data() + length_data_size))
    {
        onErrorOccurred(asio::error::access_denied);
        return;
    }

    // Send the buffer to the recipient.
    asio::async_write(socket_,
                      asio::buffer(write_.buffer.data(), write_.buffer.size()),
                      [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        WriteQueue& work_queue = write_.work_queue;

        DCHECK(!work_queue.empty());

        if (error_code)
        {
            onErrorOccurred(error_code);
            return;
        }

        // Delete the sent message from the queue.
        work_queue.pop();

        // If the queue is not empty, then we send the following message.
        if (work_queue.empty() && !reloadWriteQueue())
            return;

        doWrite();
    });
}

} // namespace net
