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

#ifndef BASE_NET_NETWORK_CHANNEL_H
#define BASE_NET_NETWORK_CHANNEL_H

#include "base/memory/byte_array.h"
#include "base/net/variable_size.h"
#include "base/net/write_task.h"
#include "base/peer/host_id.h"

#include <asio/ip/tcp.hpp>
#include <asio/high_resolution_timer.hpp>

#include <queue>

namespace base {

class NetworkChannelProxy;
class Location;
class MessageEncryptor;
class MessageDecryptor;
class NetworkServer;

class NetworkChannel
{
public:
    // Constructor available for client.
    NetworkChannel();
    ~NetworkChannel();

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    enum class ErrorCode
    {
        // Unknown error.
        UNKNOWN,

        // No error.
        SUCCESS,

        // Violation of the communication protocol.
        INVALID_PROTOCOL,

        // Cryptography error (message encryption or decryption failed).
        ACCESS_DENIED,

        // An error occurred with the network (e.g., the network cable was accidentally plugged out).
        NETWORK_ERROR,

        // The connection was refused by the peer (or timed out).
        CONNECTION_REFUSED,

        // The remote host closed the connection.
        REMOTE_HOST_CLOSED,

        // The host address was not found.
        SPECIFIED_HOST_NOT_FOUND,

        // The socket operation timed out.
        SOCKET_TIMEOUT,

        // The address specified is already in use and was set to be exclusive.
        ADDRESS_IN_USE,

        // The address specified does not belong to the host.
        ADDRESS_NOT_AVAILABLE
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void onConnected() = 0;
        virtual void onDisconnected(ErrorCode error_code) = 0;
        virtual void onMessageReceived(const ByteArray& buffer) = 0;
        virtual void onMessageWritten(size_t pending) = 0;
    };

    std::shared_ptr<NetworkChannelProxy> channelProxy();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setEncryptor(std::unique_ptr<MessageEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<MessageDecryptor> decryptor);

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
    void send(ByteArray&& buffer);

    // Disable or enable the algorithm of Nagle.
    bool setNoDelay(bool enable);

    // Enables or disables sending keep alive packets.
    // If the |enable| is set to true, TCP keep-alive is enabled. If |enable| is false, then
    // disabled and |time| and |interval| are ignored.
    // |time| specifies the timeout, in milliseconds, with no activity until the first keep-alive
    // packet is sent.
    // |interval| specifies the interval, in milliseconds, between when successive keep-alive
    // packets are sent if no acknowledgement is received.
    bool setTcpKeepAlive(bool enable,
                         const Milliseconds& time = Milliseconds(45000),
                         const Milliseconds& interval = Milliseconds(5000));
    bool setOwnKeepAlive(bool enable,
                         const Seconds& interval = Seconds(45),
                         const Seconds& timeout = Seconds(15));

    bool setReadBufferSize(size_t size);
    bool setWriteBufferSize(size_t size);

    size_t pendingMessages() const { return write_queue_.size(); }
    int64_t totalRx() const { return total_rx_; }
    int64_t totalTx() const { return total_tx_; }
    int speedRx();
    int speedTx();

    base::HostId hostId() const { return host_id_; }
    void setHostId(base::HostId host_id) { host_id_ = host_id; }

    // Converts an error code to a human readable string.
    // Does not support localization. Used for logs.
    static std::string errorToString(ErrorCode error_code);

protected:
    friend class NetworkServer;
    friend class RelayPeer;

    // Constructor available for server. An already connected socket is being moved.
    explicit NetworkChannel(asio::ip::tcp::socket&& socket);

    // Disconnects to remote host. The method is not available for an external call.
    // To disconnect, you must destroy the channel by calling the destructor.
    void disconnect();

private:
    friend class NetworkChannelProxy;

    enum class ReadState
    {
        IDLE,                // No reads are in progress right now.
        READ_SIZE,           // Reading the message size.
        READ_SERVICE_HEADER, // Reading the contents of the service header.
        READ_SERVICE_DATA,   // Reading the contents of the service data.
        READ_USER_DATA,      // Reading the contents of the user data.
        PENDING              // There is a message about which we did not notify.
    };

    enum ServiceMessageType
    {
        KEEP_ALIVE = 1
    };

    enum KeepAliveFlags
    {
        KEEP_ALIVE_PONG = 0,
        KEEP_ALIVE_PING = 1
    };

    struct ServiceHeader
    {
        uint8_t type;      // Type of service packet (see ServiceDataType).
        uint8_t flags;     // Flags bitmask (depends on the type).
        uint8_t reserved1; // Reserved.
        uint8_t reserved2; // Reserved.
        uint32_t length;   // Additional data size.
    };

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);
    void onMessageWritten();
    void onMessageReceived();

    void addWriteTask(WriteTask::Type type, ByteArray&& data);

    void doWrite();
    void onWrite(const std::error_code& error_code, size_t bytes_transferred);

    void doReadSize();
    void onReadSize(const std::error_code& error_code, size_t bytes_transferred);

    void doReadUserData(size_t length);
    void onReadUserData(const std::error_code& error_code, size_t bytes_transferred);

    void doReadServiceHeader();
    void onReadServiceHeader(const std::error_code& error_code, size_t bytes_transferred);

    void doReadServiceData(size_t length);
    void onReadServiceData(const std::error_code& error_code, size_t bytes_transferred);

    void onKeepAliveInterval(const std::error_code& error_code);
    void onKeepAliveTimeout(const std::error_code& error_code);
    void sendKeepAlive(uint8_t flags, const void* data, size_t size);

    void addTxBytes(size_t bytes_count);
    void addRxBytes(size_t bytes_count);

    std::shared_ptr<NetworkChannelProxy> proxy_;
    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;

    std::unique_ptr<asio::high_resolution_timer> keep_alive_timer_;
    Seconds keep_alive_interval_;
    Seconds keep_alive_timeout_;
    ByteArray keep_alive_counter_;
    TimePoint keep_alive_timestamp_;

    Listener* listener_ = nullptr;
    bool connected_ = false;
    bool paused_ = true;

    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    std::queue<WriteTask> write_queue_;
    VariableSizeWriter variable_size_writer_;
    ByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;
    VariableSizeReader variable_size_reader_;
    ByteArray read_buffer_;
    ByteArray decrypt_buffer_;

    int64_t total_tx_ = 0;
    int64_t total_rx_ = 0;

    TimePoint begin_time_tx_;
    int64_t bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    int64_t bytes_rx_ = 0;
    int speed_rx_ = 0;

    base::HostId host_id_ = base::kInvalidHostId;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannel);
};

} // namespace base

#endif // BASE_NET_NETWORK_CHANNEL_H
