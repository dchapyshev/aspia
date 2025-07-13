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

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include <chrono>

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include "base/net/variable_size.h"
#include "base/net/write_task.h"
#include "base/peer/authenticator.h"
#include "base/peer/host_id.h"

namespace base {

class Location;
class MessageEncryptor;
class MessageDecryptor;
class TcpServer;

class TcpChannel final : public QObject
{
    Q_OBJECT

public:
    // Constructor available for client.
    explicit TcpChannel(Authenticator* authenticator, QObject* parent = nullptr);
    ~TcpChannel() final;

    static const quint32 kMaxMessageSize;

    enum class ErrorCode
    {
        // Unknown error.
        UNKNOWN,

        // No error.
        SUCCESS,

        // Violation of the communication protocol.
        INVALID_PROTOCOL,

        // Authentication error.
        ACCESS_DENIED,

        // Cryptography error (message encryption or decryption failed).
        CRYPTO_ERROR,

        // Session type is not allowed.
        SESSION_DENIED,

        // Version mismatch.
        VERSION_ERROR,

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
    Q_ENUM(ErrorCode)

    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    // Gets the address of the remote host as a string.
    QString peerAddress() const;

    // Connects to a host at the specified address and port.
    void connectTo(const QString& address, quint16 port);

    // Returns true if the channel is connected and false if not connected.
    bool isConnected() const { return connected_; }

    // Returns true if the channel has already been fully authenticated.
    bool isAuthenticated() const { return authenticated_; }

    // Returns true if the channel is paused and false if not. If the channel is not connected,
    // then the return value is undefined.
    bool isPaused() const { return paused_; }

    // Pauses the channel. After calling the method, new messages will not be read from the socket.
    // If at the time the method was called, the message was read, then notification of this
    // message will be received only after calling method resume().
    void pause();

    // After calling the method, reading new messages will continue.
    void resume();

    // Sending a message. After the call, the message will be added to the queue to be sent.
    void send(quint8 channel_id, const QByteArray& buffer);

    bool setReadBufferSize(int size);
    bool setWriteBufferSize(int size);

    size_t pendingMessages() const { return write_queue_.size(); }

    base::HostId hostId() const { return host_id_; }
    void setHostId(base::HostId host_id) { host_id_ = host_id; }

    qint64 totalRx() const { return total_rx_; }
    qint64 totalTx() const { return total_tx_; }
    int speedRx();
    int speedTx();

    QVersionNumber peerVersion() const { return version_; }
    QString peerOsName() const { return os_name_; }
    QString peerComputerName() const { return computer_name_; }
    QString peerDisplayName() const { return display_name_; }
    QString peerArchitecture() const { return architecture_; }
    QString peerUserName() const { return user_name_; }
    quint32 peerSessionType() const { return session_type_; }

signals:
    void sig_connected();
    void sig_authenticated();
    void sig_errorOccurred(ErrorCode error_code);
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);
    void sig_messageWritten(quint8 channel_id, size_t pending);

protected:
    friend class TcpServer;
    friend class RelayPeer;

    // Constructor available for server. An already connected socket is being moved.
    TcpChannel(asio::ip::tcp::socket&& socket, Authenticator* authenticator, QObject* parent);

    // Starts authentication. In the client channel, it starts automatically when a connection is
    // established. In the server channel, it is started by the RelayPeer or TcpServer.
    void doAuthentication();

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

    void onKeyChanged();
    void onAuthenticatorMessage(const QByteArray& data);
    void onAuthenticatorFinished(Authenticator::ErrorCode error_code);

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

    void addTxBytes(size_t bytes_count);
    void addRxBytes(size_t bytes_count);

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
    std::unique_ptr<asio::ip::tcp::resolver> resolver_;

    QTimer* keep_alive_timer_ = nullptr;
    KeepAliveTimerType keep_alive_timer_type_ = KEEP_ALIVE_INTERVAL;
    QByteArray keep_alive_counter_;
    TimePoint keep_alive_timestamp_;

    bool connected_ = false;
    bool authenticated_ = false;
    bool paused_ = true;

    QPointer<Authenticator> authenticator_;
    std::unique_ptr<MessageEncryptor> encryptor_;
    std::unique_ptr<MessageDecryptor> decryptor_;

    QVersionNumber version_;
    QString os_name_;
    QString computer_name_;
    QString display_name_;
    QString architecture_;
    QString user_name_;
    quint32 session_type_ = 0;

    WriteQueue write_queue_;
    VariableSizeWriter variable_size_writer_;
    QByteArray write_buffer_;

    ReadState state_ = ReadState::IDLE;
    VariableSizeReader variable_size_reader_;
    QByteArray read_buffer_;
    QByteArray decrypt_buffer_;

    base::HostId host_id_ = base::kInvalidHostId;
    bool is_channel_id_supported_ = false;

    qint64 total_tx_ = 0;
    qint64 total_rx_ = 0;

    TimePoint begin_time_tx_;
    qint64 bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    qint64 bytes_rx_ = 0;
    int speed_rx_ = 0;

    Q_DISABLE_COPY(TcpChannel)
};

} // namespace base

Q_DECLARE_METATYPE(base::TcpChannel::ErrorCode)

#endif // BASE_NET_TCP_CHANNEL_H
