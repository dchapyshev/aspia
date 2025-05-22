//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_TCP_CHANNEL_H
#define BASE_NET_TCP_CHANNEL_H

#include <QTimer>

#include "base/net/network_channel.h"
#include "base/net/variable_size.h"
#include "base/net/write_task.h"
#include "base/peer/host_id.h"

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

namespace base {

class Location;
class MessageEncryptor;
class MessageDecryptor;
class TcpServer;

class TcpChannel final : public NetworkChannel
{
    Q_OBJECT

public:
    // Constructor available for client.
    explicit TcpChannel(QObject* parent = nullptr);
    ~TcpChannel() final;

    // Sets an instance of a class to encrypt and decrypt messages.
    // By default, a fake cryptographer is created that only copies the original message.
    // You must explicitly establish a cryptographer before or after establishing a connection.
    void setEncryptor(std::unique_ptr<MessageEncryptor> encryptor);
    void setDecryptor(std::unique_ptr<MessageDecryptor> decryptor);

    // Gets the address of the remote host as a string.
    QString peerAddress() const;

    // Connects to a host at the specified address and port.
    void connectTo(const QString& address, quint16 port);

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

    // Sending a message. After the call, the message will be added to the queue to be sent.
    void send(quint8 channel_id, const QByteArray& buffer);

    void setChannelIdSupport(bool enable);
    bool hasChannelIdSupport() const;

    bool setReadBufferSize(size_t size);
    bool setWriteBufferSize(size_t size);

    size_t pendingMessages() const { return write_queue_.size(); }

    base::HostId hostId() const { return host_id_; }
    void setHostId(base::HostId host_id) { host_id_ = host_id; }

signals:
    void sig_connected();
    void sig_disconnected(ErrorCode error_code);
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);
    void sig_messageWritten(quint8 channel_id, size_t pending);

protected:
    friend class TcpServer;
    friend class RelayPeer;

    // Constructor available for server. An already connected socket is being moved.
    TcpChannel(asio::ip::tcp::socket&& socket, QObject* parent);

    // Disconnects to remote host. The method is not available for an external call.
    // To disconnect, you must destroy the channel by calling the destructor.
    void disconnectFrom();

private:
    enum class ReadState
    {
        IDLE,                // No reads are in progress right now.
        READ_SIZE,           // Reading the message size.
        READ_SERVICE_HEADER, // Reading the contents of the service header.
        READ_SERVICE_DATA,   // Reading the contents of the service data.
        READ_USER_DATA,      // Reading the contents of the user data.
        PENDING              // There is a message about which we did not notify.
    };

    struct UserDataHeader
    {
        quint8 channel_id;
        quint8 reserved;
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

    enum KeepAliveTimerType
    {
        KEEP_ALIVE_TIMEOUT = 0,
        KEEP_ALIVE_INTERVAL = 1
    };

    struct ServiceHeader
    {
        quint8 type;      // Type of service packet (see ServiceDataType).
        quint8 flags;     // Flags bitmask (depends on the type).
        quint8 reserved1; // Reserved.
        quint8 reserved2; // Reserved.
        quint32 length;   // Additional data size.
    };

    void init();
    void setConnected(bool connected);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);

    void onMessageWritten(quint8 channel_id);
    void onMessageReceived();

    void addWriteTask(WriteTask::Type type, quint8 channel_id, const QByteArray& data);

    void doWrite();
    void doReadSize();
    void doReadUserData(size_t length);
    void doReadServiceHeader();
    void doReadServiceData(size_t length);

    void onKeepAliveTimer();
    void sendKeepAlive(quint8 flags, const void* data, size_t size);

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;

    QTimer* keep_alive_timer_ = nullptr;
    KeepAliveTimerType keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    QByteArray keep_alive_counter_;
    TimePoint keep_alive_timestamp_;

    bool connected_ = false;
    bool paused_ = true;

    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    WriteQueue write_queue_;
    VariableSizeWriter variable_size_writer_;
    QByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;
    VariableSizeReader variable_size_reader_;
    QByteArray read_buffer_;
    QByteArray decrypt_buffer_;

    base::HostId host_id_ = base::kInvalidHostId;
    bool is_channel_id_supported_ = false;

    DISALLOW_COPY_AND_ASSIGN(TcpChannel);
};

} // namespace base

#endif // BASE_NET_TCP_CHANNEL_H
