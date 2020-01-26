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

#ifndef NET__SOCKET_READER_H
#define NET__SOCKET_READER_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"

#include <asio/ip/tcp.hpp>

#include <functional>

namespace crypto {
class MessageDecryptor;
} // namespace crypto

namespace net {

class SocketReader
{
public:
    using ReadFailedCallback = std::function<void(const std::error_code&)>;
    using ReadMessageCallback = std::function<void(const base::ByteArray&)>;

    SocketReader();
    ~SocketReader();

    void attach(asio::ip::tcp::socket* socket,
                ReadMessageCallback read_message_callback,
                ReadFailedCallback read_failed_callback);
    void dettach();

    void setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor);

    void resume();
    void pause();

    bool isPaused() const;

private:
    void doReadSize();
    void onReadSize(const std::error_code& error_code, size_t bytes_transferred);

    void doReadContent();
    void onReadContent(const std::error_code& error_code, size_t bytes_transferred);

    void onMessageReceived();

    asio::ip::tcp::socket* socket_ = nullptr;
    bool paused_ = true;

    std::unique_ptr<crypto::MessageDecryptor> decryptor_;
    base::ByteArray decrypt_buffer_;

    enum class State
    {
        IDLE,        // No reads are in progress right now.
        READ_SIZE,   // Reading the message size.
        READ_CONTENT // Reading the contents of the message.
    };

    State state_ = State::IDLE;
    uint8_t one_byte_ = 0;
    base::ByteArray read_buffer_;
    size_t read_buffer_size_ = 0;
    size_t bytes_transfered_ = 0;

    ReadFailedCallback read_failed_callback_;
    ReadMessageCallback read_message_callback_;

    DISALLOW_COPY_AND_ASSIGN(SocketReader);
};

} // namespace net

#endif // NET__SOCKET_READER_H
