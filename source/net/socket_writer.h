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

#ifndef NET__SOCKET_WRITER_H
#define NET__SOCKET_WRITER_H

#include "base/byte_array.h"
#include "base/macros_magic.h"

#include <asio/ip/tcp.hpp>

#include <queue>
#include <memory>
#include <mutex>

namespace crypto {
class MessageEncryptor;
} // namespace crypto

namespace net {

class SocketWriter : public std::enable_shared_from_this<SocketWriter>
{
public:
    using WriteFailedCallback = std::function<void(const std::error_code&)>;
    using WriteDoneCallback = std::function<void()>;

    SocketWriter();
    ~SocketWriter();

    void attach(asio::ip::tcp::socket* socket,
                WriteDoneCallback write_done_callback,
                WriteFailedCallback write_failed_callback);
    void dettach();

    void setEncryptor(std::unique_ptr<crypto::MessageEncryptor> encryptor);
    void send(base::ByteArray&& buffer);

private:
    void doWrite();
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);

#if defined(USE_TBB)
    using QueueAllocator = tbb::scalable_allocator<base::ByteArray>;
#else // defined(USE_TBB)
    using QueueAllocator = std::allocator<base::ByteArray>;
#endif // defined(USE_*)

    using WriteQueue = std::queue<base::ByteArray, std::deque<base::ByteArray, QueueAllocator>>;

    asio::ip::tcp::socket* socket_ = nullptr;
    std::unique_ptr<crypto::MessageEncryptor> encryptor_;

    WriteQueue write_queue_;
    base::ByteArray write_buffer_;

    WriteDoneCallback write_done_callback_;
    WriteFailedCallback write_failed_callback_;
};

} // namespace net

#endif // NET__SOCKET_WRITER_H
