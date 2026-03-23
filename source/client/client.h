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
#include <QVariant>

#include "client/client_session_state.h"
#include "client/router_manager.h"
#include "base/crypto/key_pair.h"
#include "base/net/tcp_channel.h"

class QTimer;

namespace base {
class StunPeer;
class UdpChannel;
} // namespace base

namespace client {

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
        ROUTER_CONNECTING,
        ROUTER_CONNECTED,
        ROUTER_ERROR,
        HOST_CONNECTING,
        HOST_CONNECTED,
        HOST_DISCONNECTED,
        WAIT_FOR_ROUTER,
        WAIT_FOR_ROUTER_TIMEOUT,
        WAIT_FOR_HOST,
        WAIT_FOR_HOST_TIMEOUT,
        VERSION_MISMATCH,
        LEGACY_HOST // Remove this after support for versions below 3.0.0 ends.
    };
    Q_ENUM(Status)

    enum class UdpConnectPhase
    {
        NONE,
        DIRECT_LAN,
        HOLE_PUNCHING,
        HOLE_PUNCHING_RETRY
    };
    Q_ENUM(UdpConnectPhase)

signals:
    void sig_statusChanged(client::Client::Status status, const QVariant& data = QVariant());
    void sig_showSessionWindow();

protected:
    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onSessionStarted() = 0;
    virtual void onSessionMessageReceived(const QByteArray& buffer) = 0;
    virtual void onServiceMessageReceived(const QByteArray& buffer) = 0;

    // Sends outgoing message.
    void sendSessionMessage(const QByteArray& message);
    void sendServiceMessage(const QByteArray& message);

    // Methods for obtaining network metrics.
    qint64 totalTcpRx() const;
    qint64 totalTcpTx() const;
    int speedTcpRx();
    int speedTcpTx();
    qint64 totalUdpRx() const;
    qint64 totalUdpTx() const;
    int speedUdpRx();
    int speedUdpTx();

private slots:
    void onTcpConnected();
    void onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpReady();
    void onUdpErrorOccurred();
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onRouterConnected(const QVersionNumber& router_version);
    void onHostAwaiting();
    void onHostConnected(bool peer_address_equals, const QString& stun_host, quint16 stun_port);
    void onRouterErrorOccurred(const client::RouterManager::Error& error);

private:
    void delayedReconnect();
    void tcpChannelReady();
    void startUdpHolePunching();
    void startDirectUdp(qintptr socket, const QString& address, quint16 port);

    bool is_legacy_mode_ = false;
    QTimer* timeout_timer_ = nullptr;
    QTimer* reconnect_timer_ = nullptr;
    RouterManager* router_controller_ = nullptr;
    base::TcpChannel* tcp_channel_ = nullptr;
    base::UdpChannel* udp_channel_ = nullptr;
    base::StunPeer* stun_peer_ = nullptr;
    base::KeyPair udp_key_pair_;
    QByteArray udp_iv_;

    UdpConnectPhase udp_phase_ = UdpConnectPhase::NONE;
    QString stun_host_;
    quint16 stun_port_ = 0;

    std::shared_ptr<SessionState> session_state_;

    enum class State { CREATED, STARTED, STOPPPED };
    State state_ = State::CREATED;

    bool is_connected_to_router_ = false;
    bool udp_ready_ = false;
};

} // namespace client

Q_DECLARE_METATYPE(client::Client::Status)

#endif // CLIENT_CLIENT_H
