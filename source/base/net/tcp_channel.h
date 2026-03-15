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
#include <QVersionNumber>

#include <chrono>

namespace base {

class TcpChannel : public QObject
{
    Q_OBJECT

public:
    enum class Type { DIRECT, RELAY };
    Q_ENUM(Type)

    TcpChannel(Type type, QObject* parent);
    ~TcpChannel() override = default;

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

    virtual QString peerAddress() const = 0;
    virtual void connectTo(const QString& address, quint16 port) = 0;
    virtual bool isConnected() const = 0;
    virtual bool isAuthenticated() const = 0;
    virtual bool isPaused() const = 0;
    virtual void setPaused(bool enable) = 0;
    virtual void send(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual bool setReadBufferSize(int size) = 0;
    virtual bool setWriteBufferSize(int size) = 0;
    virtual qint64 pendingBytes() const = 0;

    quint32 instanceId() const { return instance_id_; }
    Type type() const { return type_; }
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
    void sig_errorOccurred(base::TcpChannel::ErrorCode error_code);
    void sig_messageReceived(quint8 channel_id, const QByteArray& buffer);
    void sig_messageWritten(quint8 channel_id);

protected:
    friend class TcpServerLegacy;
    friend class TcpServer;
    friend class RelayPeer;

    virtual void doAuthentication() = 0;

    void addTxBytes(size_t bytes_count);
    void addRxBytes(size_t bytes_count);

    QVersionNumber version_;
    QString os_name_;
    QString computer_name_;
    QString display_name_;
    QString architecture_;
    QString user_name_;
    quint32 session_type_ = 0;

private:
    const quint32 instance_id_;
    const Type type_;

    qint64 total_tx_ = 0;
    qint64 total_rx_ = 0;

    TimePoint begin_time_tx_;
    qint64 bytes_tx_ = 0;
    int speed_tx_ = 0;

    TimePoint begin_time_rx_;
    qint64 bytes_rx_ = 0;
    int speed_rx_ = 0;

    Q_DISABLE_COPY_MOVE(TcpChannel)
};

} // namespace base

Q_DECLARE_METATYPE(base::TcpChannel::ErrorCode)

#endif // BASE_NET_TCP_CHANNEL_H
