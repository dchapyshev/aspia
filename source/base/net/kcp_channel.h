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

#ifndef BASE_NET_KCP_CHANNEL_H
#define BASE_NET_KCP_CHANNEL_H

#include "base/location.h"
#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "base/net/network_channel.h"
#include "base/net/write_task.h"
#include "third_party/kcp/ikcp.h"

#include <cstdint>
#include <queue>
#include <string>

#include <asio/high_resolution_timer.hpp>
#include <asio/ip/udp.hpp>

namespace base {

class KcpChannelProxy;
class MessageEncryptor;
class MessageDecryptor;

class KcpChannel : public NetworkChannel
{
public:
    KcpChannel();
    ~KcpChannel() override;

    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void onKcpConnected() = 0;
        virtual void onKcpDisconnected(ErrorCode error_code) = 0;
        virtual void onKcpMessageReceived(uint8_t channel_id, const ByteArray& buffer) = 0;
        virtual void onKcpMessageWritten(uint8_t channel_id, size_t pending) = 0;
    };

    std::shared_ptr<KcpChannelProxy> channelProxy();

    // Sets an instance of the class to receive connection status notifications or new messages.
    // You can change this in the process.
    void setListener(Listener* listener);

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setEncryptor(std::unique_ptr<MessageEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<MessageDecryptor> decryptor);

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
    void send(uint8_t channel_id, ByteArray&& buffer);

    bool setKeepAlive(bool enable,
                      const Seconds& interval = Seconds(45),
                      const Seconds& timeout = Seconds(15));

    size_t pendingMessages() const;

    // Converts an error code to a human readable string.
    // Does not support localization. Used for logs.
    static std::string errorToString(ErrorCode error_code);

protected:
    // Disconnects to remote host. The method is not available for an external call.
    // To disconnect, you must destroy the channel by calling the destructor.
    void disconnect();

private:
    friend class KcpChannelProxy;

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

    using ReadCompleteCallback = std::function<void(size_t bytes_transferred)>;

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);
    void onMessageWritten(uint8_t channel_id);
    void onMessageReceived();

    void addWriteTask(WriteTask::Type type, uint8_t channel_id, ByteArray&& data);
    void doWrite();

    void doReadSize();
    void onReadSize(size_t bytes_transferred);

    void doReadUserData(size_t length);
    void onReadUserData(size_t bytes_transferred);

    void doReadServiceHeader();
    void onReadServiceHeader(size_t bytes_transferred);

    void doReadServiceData(size_t length);
    void onReadServiceData(size_t bytes_transferred);

    void doRead(asio::mutable_buffer buffer, ReadCompleteCallback callback);

    void startUpdateTimer(uint32_t timeout);
    void stopUpdateTimer();
    void onUpdateTimeout(const std::error_code& error_code);
    void onUpdate(uint32_t time);
    void initKcp();

    void onKeepAliveInterval(const std::error_code& error_code);
    void onKeepAliveTimeout(const std::error_code& error_code);
    void sendKeepAlive(uint8_t flags, const void* data, size_t size);

    void onDataWrite(const char* buf, size_t len);
    static int onDataWriteCallback(const char* buf, int len, struct IKCPCB* kcp, void* user);

    Listener* listener_ = nullptr;
    bool connected_ = false;
    bool paused_ = true;

    std::unique_ptr<asio::high_resolution_timer> update_timer_;
    std::shared_ptr<KcpChannelProxy> proxy_;
    std::unique_ptr<asio::ip::udp::resolver> resolver_;
    asio::ip::udp::socket socket_;
    TimePoint last_service_time_;
    ikcpcb* kcp_ = nullptr;

    std::unique_ptr<asio::high_resolution_timer> keep_alive_timer_;
    Seconds keep_alive_interval_;
    Seconds keep_alive_timeout_;
    ByteArray keep_alive_counter_;
    TimePoint keep_alive_timestamp_;

    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    std::queue<WriteTask> write_queue_;
    ByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;

    // The buffer into which data is read from the UDP socket.
    std::array<char, 4096> input_buffer_;

    // The buffer into which data is read from the KCP.
    ByteArray recv_buffer_;
    size_t recv_buffer_pos_ = 0;

    uint32_t read_size_ = 0;
    ByteArray read_buffer_;
    ByteArray decrypt_buffer_;

    DISALLOW_COPY_AND_ASSIGN(KcpChannel);
};

} // namespace base

#endif // BASE_NET_KCP_CHANNEL_H
