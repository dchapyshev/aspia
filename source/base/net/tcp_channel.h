//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QObject>
#include <QQueue>
#include <QVersionNumber>

#include "base/logging.h"
#include "base/scoped_qpointer.h"
#include "base/shared_pointer.h"
#include "base/time_types.h"

class Authenticator;
class Location;
class RelayPeer;
class StreamDecryptor;
class StreamEncryptor;
class TcpServer;

class TcpChannel : public QObject
{
    Q_OBJECT

public:
    enum class Type { DIRECT, RELAY };
    Q_ENUM(Type)

    TcpChannel(Type type, Authenticator* authenticator, QObject* parent);
    ~TcpChannel() override;

    static const quint32 kMaxMessageSize;

    // Upper bound on a single handshake frame. Real auth messages stay well under 4 KB
    // (the largest is SrpServerKeyExchange with 8192-bit N/B - about 3 KB). The lower cap
    // applies until isAuthenticated() returns true, so an unauthenticated peer cannot force
    // the channel to allocate the full |kMaxMessageSize| RX buffer.
    static const quint32 kMaxAuthMessageSize;

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
        ADDRESS_NOT_AVAILABLE,

        // The proxy server requires authentication or rejected the credentials.
        PROXY_AUTH_REQUIRED,

        // The proxy server refused to open the tunnel or sent an invalid response.
        PROXY_ERROR,

        // The proxy server could not connect to the destination host.
        PROXY_DESTINATION_UNREACHABLE,

        // The key of the host is not trusted yet or differs from the pinned one. No credentials
        // were sent.
        HOST_KEY_UNTRUSTED
    };
    Q_ENUM(ErrorCode)

    static QString errorToString(ErrorCode error_code);

    // Connects to a host at the specified address and port.
    virtual void connectTo(const QString& address, quint16 port, Seconds timeout = Seconds(30)) = 0;

    // Returns true if the channel is connected and false if not connected.
    bool isConnected() const { return connected_; }

    // Returns true if the channel has already been fully authenticated.
    bool isAuthenticated() const { return authenticated_; }

    // Returns true if the channel is paused and false if not. If the channel is not connected,
    // then the return value is undefined.
    bool isPaused() const { return paused_; }

    // Pauses or resume the channel. After calling the method, new messages will not be read from
    // the socket. If at the time the method was called with |true', the message was read, then
    // notification of this message will be received only after calling method with |false|.
    void setPaused(bool enable);

    // Sending a message. After the call, the message will be added to the queue to be sent.
    void send(quint8 channel_id, const QByteArray& buffer);

    virtual bool setReadBufferSize(int size) = 0;
    virtual bool setWriteBufferSize(int size) = 0;

    qint64 pendingBytes() const;

    // Called by the channel owner about once per second.
    void tick(TimePoint now);

    quint32 instanceId() const { return instance_id_; }
    Type type() const { return type_; }
    qint64 totalRx() const { return total_rx_; }
    qint64 totalTx() const { return total_tx_; }
    int speedRx();
    int speedTx();

    const std::string& peerAddress() const { return address_; }
    QVersionNumber peerVersion() const { return version_; }
    const std::string& peerOsName() const { return os_name_; }
    const std::string& peerComputerName() const { return computer_name_; }
    const std::string& peerDisplayName() const { return display_name_; }
    const std::string& peerArchitecture() const { return architecture_; }
    const std::string& peerUserName() const { return user_name_; }
    qint64 peerUserId() const { return user_id_; }
    quint32 peerSessionType() const { return session_type_; }

signals:
    void sig_connected();
    void sig_authenticated();
    void sig_errorOccurred(TcpChannel::ErrorCode error_code);
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);

protected:
    LOG_DECLARE_CONTEXT(TcpChannel);

    friend class TcpServer;
    friend class RelayPeer;

    struct Header
    {
        quint8 type;
        quint8 param1; // Depends on the message type.
        quint8 param2; // Not used yet.
        quint8 param3; // Not used yet.
        quint32 length;
    };

    struct IoState
    {
        bool alive = true;
        Header read_header;
        QByteArray read_buffer;
        QQueue<QByteArray> write_queue;
    };

    // The transport of the subclass. A read completes once |size| bytes are read and a write once all
    // |size| bytes are written. The subclass reports the result with onReadCompleted and
    // onWriteCompleted.
    virtual void readTransport(void* data, size_t size) = 0;
    virtual void writeTransport(const void* data, size_t size) = 0;
    virtual void closeTransport() = 0;

    // Starts authentication. A channel that connects by itself starts it once the connection is
    // established. A channel accepted by TcpServer is started by the server.
    void doAuthentication();

    // Bytes that go out as they are ahead of the first handshake frame. The relay reads its own
    // message this way before it joins the peers.
    void setPreamble(QByteArray preamble);

    // Called by the subclass once its transport is connected.
    void setConnected(const std::string& peer_address);

    // Called by the subclass once a transport operation completes.
    void onReadCompleted(const std::error_code& error_code, size_t bytes_transferred);
    void onWriteCompleted(const std::error_code& error_code, size_t bytes_transferred);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onErrorOccurred(const Location& location, ErrorCode error_code);

    // The subclass captures the result in each transport handler so the buffers stay valid until
    // the handler runs, and checks |alive| before touching the channel.
    SharedPointer<IoState> ioState() const { return io_; }

private:
    enum class ReadState
    {
        IDLE,        // No reads are in progress right now.
        READ_HEADER, // Reading the contents of the header.
        READ_DATA,   // Reading the contents of the data.
        PENDING      // There is a message about which we did not notify.
    };

    enum MessageType
    {
        KEEP_ALIVE = 1,
        AUTH_DATA  = 2,
        USER_DATA  = 3
    };

    enum KeepAliveFlags
    {
        KEEP_ALIVE_PONG = 0,
        KEEP_ALIVE_PING = 1
    };

    // Keep-alive state machine driven by the owner through tick().
    enum class KeepAliveState
    {
        INACTIVE,      // Channel is paused or not authenticated yet.
        WAIT_INTERVAL, // Waiting for the idle interval to elapse before sending a PING.
        WAIT_PONG      // PING sent; the PONG must arrive before the deadline.
    };

    void init();
    void setDisconnected();
    void onMessageReceived();
    void addWriteTask(quint8 type, quint8 param, const QByteArray& data, bool encrypted);
    void doWrite();
    void doReadHeader();
    void doReadData();
    void startKeepAliveInterval();
    void addTxBytes(size_t bytes_count);
    void addRxBytes(size_t bytes_count);

    const quint32 instance_id_;
    const Type type_;

    std::string address_;
    QVersionNumber version_;
    std::string os_name_;
    std::string computer_name_;
    std::string display_name_;
    std::string architecture_;
    std::string user_name_;
    qint64 user_id_ = 0;
    quint32 session_type_ = 0;

    qint64 total_tx_ = 0;
    qint64 total_rx_ = 0;

    TimePoint begin_time_tx_;
    qint64 bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    qint64 bytes_rx_ = 0;
    int speed_rx_ = 0;

    SharedPointer<IoState> io_ { new IoState() };

    KeepAliveState keep_alive_state_ = KeepAliveState::INACTIVE;
    TimePoint keep_alive_deadline_;
    QByteArray keep_alive_counter_;
    // Set to true on any successfully received message; checked and cleared when the
    // idle interval elapses. Lets us avoid sending PINGs when the peer is obviously
    // alive due to incoming traffic, without touching the deadline on every message.
    bool rx_since_last_check_ = false;

    bool connected_ = false;
    bool authenticated_ = false;
    bool paused_ = true;

    ScopedQPointer<Authenticator> authenticator_;
    std::unique_ptr<StreamEncryptor> encryptor_;
    std::unique_ptr<StreamDecryptor> decryptor_;

    QList<QByteArray> write_pool_;
    QByteArray preamble_;

    ReadState state_ = ReadState::IDLE;
    QByteArray decrypt_buffer_;

    Q_DISABLE_COPY_MOVE(TcpChannel)
};

Q_DECLARE_METATYPE(TcpChannel::ErrorCode)

#endif // BASE_NET_TCP_CHANNEL_H
