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

#ifndef NET__NETWORK_CHANNEL_H
#define NET__NETWORK_CHANNEL_H

#include "base/byte_array.h"
#include "base/macros_magic.h"
#include "net/network_error.h"

#include <asio/ip/tcp.hpp>

#include <memory>
#include <mutex>
#include <queue>

namespace crypto {
class Cryptor;
} // namespace crypto

namespace net {

class ChannelProxy;
class Listener;
class Server;

class Channel
{
public:
    // Constructor available for client.
    explicit Channel(asio::io_context& io_context);
    ~Channel();

    std::shared_ptr<ChannelProxy> channelProxy();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setCryptor(std::unique_ptr<crypto::Cryptor> cryptor);

    // Gets the address of the remote host as a string.
    std::u16string peerAddress() const;

    // Connects to a host at the specified address and port.
    void connect(std::u16string_view address, uint16_t port);

    // Disconnects to remote host.
    void disconnect();

    // Returns true if the channel is connected and false if not connected.
    bool isConnected() const;

    // Returns true if the channel is paused and false if not. If the channel is not connected,
    // then the return value is undefined.
    bool isPaused() const;

    // Pauses the channel. After calling the method, new messages will not be read from the socket.
    // If at the time the method was called, the message was read, then notification of this
    // message will be received only after calling method resume().
    void pause();

    // After calling the method, reading new messages will continue.
    void resume();

    // Sending a message. The method call is thread safe. After the call, the message will be added
    // to the queue to be sent.
    void send(base::ByteArray&& buffer);

protected:
    friend class Server;

    // Constructor available for server. An already connected socket is being moved.
    Channel(asio::io_context& io_context, asio::ip::tcp::socket&& socket);

private:
    void onErrorOccurred(const std::error_code& error_code);
    void doReadSize();
    void doReadContent();
    void onMessageReceived();
    bool reloadWriteQueue();
    void scheduleWrite();
    void doWrite();

    asio::io_context& io_context_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<crypto::Cryptor> cryptor_;

    std::shared_ptr<ChannelProxy> proxy_;
    Listener* listener_ = nullptr;

    bool is_connected_ = false;
    bool is_paused_ = true;

#if defined(USE_TBB)
    using QueueAllocator = tbb::scalable_allocator<base::ByteArray>;
#else // defined(USE_TBB)
    using QueueAllocator = std::allocator<base::ByteArray>;
#endif // defined(USE_*)

    using QueueContainer = std::deque<base::ByteArray, QueueAllocator>;

    class WriteQueue : public std::queue<base::ByteArray, QueueContainer>
    {
    public:
        void fastSwap(WriteQueue& queue)
        {
            // Calls std::deque::swap.
            c.swap(queue.c);
        }
    };

    struct WriteContext
    {
        WriteQueue incoming_queue;
        std::mutex incoming_queue_lock;

        WriteQueue work_queue;
        base::ByteArray buffer;
    };

    struct ReadContext
    {
        uint8_t one_byte = 0;
        base::ByteArray buffer;
        size_t buffer_size = 0;
        size_t bytes_transfered = 0;
    };

    WriteContext write_;
    ReadContext read_;
};

} // namespace net

#endif // NET__NETWORK_CHANNEL_H
