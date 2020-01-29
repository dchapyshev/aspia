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

#ifndef NET__NETWORK_CHANNEL_H
#define NET__NETWORK_CHANNEL_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "base/memory/scalable_queue.h"
#include "net/network_error.h"

#include <asio/ip/tcp.hpp>

#include <memory>

namespace base {
class Location;
} // namespace base

namespace crypto {
class MessageEncryptor;
class MessageDecryptor;
} // namespace crypto

namespace net {

class ChannelProxy;
class Listener;
class Server;
class SocketConnector;
class SocketReader;

class Channel
{
public:
    // Constructor available for client.
    Channel();
    ~Channel();

    std::shared_ptr<ChannelProxy> channelProxy();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setEncryptor(std::unique_ptr<crypto::MessageEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<crypto::MessageDecryptor> decryptor);

    // Gets the address of the remote host as a string.
    std::u16string peerAddress() const;

    // Connects to a host at the specified address and port.
    void connect(std::u16string_view address, uint16_t port);

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

    // Disable or enable the algorithm of Nagle.
    bool setNoDelay(bool enable);

    // Enables or disables sending keep alive packets.
    // If the |enable| is set to true, TCP keep-alive is enabled. If |enable| is false, then
    // disabled and |time| and |interval| are ignored.
    // |time| specifies the timeout, in milliseconds, with no activity until the first keep-alive
    // packet is sent.
    // |interval| specifies the interval, in milliseconds, between when successive keep-alive
    // packets are sent if no acknowledgement is received.
    bool setKeepAlive(bool enable,
                      const std::chrono::milliseconds& time = std::chrono::milliseconds(),
                      const std::chrono::milliseconds& interval = std::chrono::milliseconds());

protected:
    friend class Server;

    // Constructor available for server. An already connected socket is being moved.
    explicit Channel(asio::ip::tcp::socket&& socket);

    // Initializes internal components.
    void init();

    // Disconnects to remote host. The method is not available for an external call.
    // To disconnect, you must destroy the channel by calling the destructor.
    void disconnect();

private:
    friend class ChannelProxy;

    void onConnected();
    void onErrorOccurred(const base::Location& location, const std::error_code& error_code);
    void onMessageReceived(const base::ByteArray& buffer);
    void onMessageWritten();

    void doWrite();
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;

    Listener* listener_ = nullptr;
    bool connected_ = false;

    std::unique_ptr<SocketConnector> connector_;
    std::unique_ptr<SocketReader> reader_;
    std::shared_ptr<ChannelProxy> proxy_;

    std::unique_ptr<crypto::MessageEncryptor> encryptor_;

    base::ScalableQueue<base::ByteArray> write_queue_;
    base::ByteArray write_buffer_;

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace net

#endif // NET__NETWORK_CHANNEL_H
