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

#include "net/socket_reader.h"

#include "base/location.h"
#include "base/logging.h"
#include "crypto/message_decryptor_fake.h"
#include "net/socket_constants.h"

#include <asio/read.hpp>

namespace net {

SocketReader::SocketReader()
    : decryptor_(std::make_unique<crypto::MessageDecryptorFake>())
{
    // Nothing
}

SocketReader::~SocketReader() = default;

void SocketReader::attach(asio::ip::tcp::socket* socket,
                          ReadMessageCallback read_message_callback,
                          ReadFailedCallback read_failed_callback)
{
    socket_ = socket;
    read_message_callback_ = std::move(read_message_callback);
    read_failed_callback_ = std::move(read_failed_callback);
}

void SocketReader::dettach()
{
    socket_ = nullptr;
}

void SocketReader::setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor)
{
    decryptor_ = std::move(decryptor);
}

void SocketReader::resume()
{
    if (!paused_)
        return;

    paused_ = false;

    // We already have an incomplete read operation.
    if (state_ != State::IDLE)
        return;

    // If we have a message that was received before the pause command.
    if (read_buffer_size_)
        onMessageReceived();

    DCHECK_EQ(bytes_transfered_, 0);
    DCHECK_EQ(read_buffer_size_, 0);

    doReadSize();
}

void SocketReader::pause()
{
    paused_ = true;
}

bool SocketReader::isPaused() const
{
    return paused_;
}

void SocketReader::doReadSize()
{
    state_ = State::READ_SIZE;

    asio::async_read(*socket_,
                     asio::buffer(&one_byte_, 1),
                     std::bind(&SocketReader::onReadSize,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void SocketReader::onReadSize(const std::error_code& error_code, size_t bytes_transferred)
{
    if (!socket_)
        return;

    if (error_code)
    {
        read_failed_callback_(FROM_HERE, error_code);
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

void SocketReader::doReadContent()
{
    state_ = State::READ_CONTENT;

    if (!read_buffer_size_ || read_buffer_size_ > kMaxMessageSize)
    {
        read_failed_callback_(FROM_HERE, asio::error::message_size);
        return;
    }

    if (read_buffer_.capacity() < read_buffer_size_)
        read_buffer_.reserve(read_buffer_size_);

    read_buffer_.resize(read_buffer_size_);

    asio::async_read(*socket_,
                     asio::buffer(read_buffer_.data(), read_buffer_.size()),
                     std::bind(&SocketReader::onReadContent,
                               this,
                               std::placeholders::_1,
                               std::placeholders::_2));
}

void SocketReader::onReadContent(const std::error_code& error_code, size_t bytes_transferred)
{
    if (!socket_)
        return;

    if (error_code)
    {
        read_failed_callback_(FROM_HERE, error_code);
        return;
    }

    DCHECK_EQ(bytes_transferred, read_buffer_.size());

    if (paused_)
    {
        state_ = State::IDLE;
        return;
    }

    onMessageReceived();

    if (paused_)
    {
        state_ = State::IDLE;
        return;
    }

    DCHECK_EQ(bytes_transfered_, 0);
    DCHECK_EQ(read_buffer_size_, 0);

    doReadSize();
}

void SocketReader::onMessageReceived()
{
    const size_t decrypt_buffer_size = decryptor_->decryptedDataSize(read_buffer_.size());

    if (decrypt_buffer_.capacity() < decrypt_buffer_size)
        decrypt_buffer_.reserve(decrypt_buffer_size);

    decrypt_buffer_.resize(decrypt_buffer_size);

    if (!decryptor_->decrypt(read_buffer_.data(), read_buffer_.size(), decrypt_buffer_.data()))
    {
        read_failed_callback_(FROM_HERE, asio::error::access_denied);
        return;
    }

    read_message_callback_(decrypt_buffer_);

    bytes_transfered_ = 0;
    read_buffer_size_ = 0;
}

} // namespace net
