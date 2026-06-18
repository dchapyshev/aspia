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

#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H

#include <QObject>
#include <QStringList>
#include <QVariant>

#include <list>
#include <memory>

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"
#include "client/session_state.h"

class RelayPeer;
class SessionKeeper;
class UdpAttempt;
class UdpChannel;

namespace proto::peer {
class DirectUdpRequest;
class StunUdpRequest;
class GatewayUdpRequest;
} // namespace proto::peer

enum class UdpMethod;

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject* parent);
    virtual ~Client() override;

    // Starts a session.
    void start();

    // Sets an instance of a class that stores session state.
    // The method must be called before calling method start().
    void setSessionState(std::shared_ptr<SessionState> session_state);
    std::shared_ptr<SessionState> sessionState() { return session_state_; }

    enum class Status
    {
        STARTED,
        STOPPED,
        NO_ROUTER,
        ROUTER_OFFLINE,
        ROUTER_ERROR,
        HOST_CONNECTING,
        HOST_CONNECTED,
        HOST_DISCONNECTED,
        WAIT_FOR_HOST,
        WAIT_FOR_HOST_TIMEOUT,
        VERSION_MISMATCH,
        LEGACY_HOST // Remove this after support for versions below 3.0.0 ends.
    };
    Q_ENUM(Status)

signals:
    void sig_statusChanged(Client::Status status, const QVariant& data = QVariant());
    void sig_showSessionWindow();

protected:
    LOG_DECLARE_CONTEXT(Client);

    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onStarted() = 0;
    virtual void onMessageReceived(quint8 channel_id, const QByteArray& buffer) = 0;

    // Sends outgoing message.
    void sendMessage(quint8 channel_id, const QByteArray& message);

    // Methods for obtaining network metrics.
    qint64 totalTcpRx() const;
    qint64 totalTcpTx() const;
    int speedTcpRx();
    int speedTcpTx();
    qint64 totalUdpRx() const;
    qint64 totalUdpTx() const;
    int speedUdpRx();
    int speedUdpTx();

    // The transport of the active UDP connection, or UdpMethod::DISABLED if UDP is not in use.
    UdpMethod udpMethod() const;

    bool isLegacy() const;

private slots:
    void onTcpConnected();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpErrorOccurred();
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void tcpChannelReady();
    void readDirectUdpRequest(const proto::peer::DirectUdpRequest& request);
    void readStunUdpRequest(const proto::peer::StunUdpRequest& request);
    void readGatewayUdpRequest(const proto::peer::GatewayUdpRequest& request);
    void addAndStart(UdpAttempt* attempt);
    UdpAttempt* findAttempt(quint32 request_id);
    void eraseAttempt(quint32 request_id);
    void clearAttempts();
    void onAttemptConnected(quint32 request_id);
    void onAttemptError(quint32 request_id);
    void selectAttempt(UdpAttempt* attempt);

    bool is_legacy_mode_ = false;
    ScopedQPointer<TcpChannel> tcp_channel_;
    ScopedQPointer<UdpChannel> udp_channel_;
    ScopedQPointer<RelayPeer> relay_peer_;

    std::list<ScopedQPointer<UdpAttempt>> attempts_;
    std::shared_ptr<SessionState> session_state_;
    SessionKeeper* session_keeper_ = nullptr;

    enum class State { CREATED, STARTED, STOPPPED };
    State state_ = State::CREATED;

    bool udp_ready_ = false;
    const quint32 udp_methods_;
};

Q_DECLARE_METATYPE(Client::Status)

#endif // CLIENT_CLIENT_H
