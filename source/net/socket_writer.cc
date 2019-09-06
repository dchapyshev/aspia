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

#include "net/socket_writer.h"

#include "base/logging.h"
#include "crypto/message_encryptor_fake.h"
#include "net/socket_constants.h"

#include <asio/write.hpp>

namespace net {

SocketWriter::SocketWriter()
    : encryptor_(std::make_unique<crypto::MessageEncryptorFake>())
{
    // Nothing
}

SocketWriter::~SocketWriter() = default;

void SocketWriter::attach(asio::ip::tcp::socket* socket,
                          WriteDoneCallback write_done_callback,
                          WriteFailedCallback write_failed_callback)
{
    socket_ = socket;
    write_done_callback_ = std::move(write_done_callback);
    write_failed_callback_ = std::move(write_failed_callback);
}

void SocketWriter::dettach()
{
    socket_ = nullptr;
}

void SocketWriter::setEncryptor(std::unique_ptr<crypto::MessageEncryptor> encryptor)
{
    encryptor_ = std::move(encryptor);
}

void SocketWriter::send(base::ByteArray&& buffer)
{
    const bool schedule_write = write_queue_.empty();

    // Add the buffer to the queue for sending.
    write_queue_.emplace(std::move(buffer));

    if (schedule_write)
        doWrite();
}

void SocketWriter::doWrite()
{
    const base::ByteArray& source_buffer = write_queue_.front();
    if (source_buffer.empty())
    {
        write_failed_callback_(asio::error::message_size);
        return;
    }

    // Calculate the size of the encrypted message.
    const size_t target_data_size = encryptor_->encryptedDataSize(source_buffer.size());

    if (target_data_size > kMaxMessageSize)
    {
        write_failed_callback_(asio::error::message_size);
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
        write_failed_callback_(asio::error::access_denied);
        return;
    }

    // Send the buffer to the recipient.
    asio::async_write(*socket_,
                      asio::buffer(write_buffer_.data(), write_buffer_.size()),
                      std::bind(&SocketWriter::onWrite,
                                shared_from_this(),
                                std::placeholders::_1,
                                std::placeholders::_2));
}

void SocketWriter::onWrite(const std::error_code& error_code, size_t bytes_transferred)
{
    if (!socket_)
        return;

    DCHECK(!write_queue_.empty());

    if (error_code)
    {
        write_failed_callback_(error_code);
        return;
    }

    write_done_callback_();

    // Delete the sent message from the queue.
    write_queue_.pop();

    // If the queue is not empty, then we send the following message.
    if (write_queue_.empty())
        return;

    doWrite();
}

} // namespace net
