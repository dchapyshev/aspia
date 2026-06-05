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

#include <memory>
#include <optional>

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/scoped_qpointer.h"
#include "client/session_state.h"

class RelayPeer;
class StunPeer;
class UdpChannel;
class UpnpPortMapper;

namespace proto::peer {
class DirectUdpRequest;
class StunUdpRequest;
class UpnpUdpRequest;
} // namespace proto::peer

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
    bool isLegacy() const;

private slots:
    void onTcpConnected();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onUdpReady();
    void onUdpErrorOccurred();
    void onUdpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    struct PendingUdp
    {
        QString address;
        quint16 port = 0;
        quint32 encryptions = 0;
        QByteArray public_key;
        QByteArray iv;
        quint32 request_id = 0;
    };

    struct UdpChannelKeys
    {
        QByteArray public_key; // Client's public key, sent back to the host in the reply.
        QByteArray iv;         // Client's IV, sent back to the host in the reply.
        quint32 encryption = 0;
    };

    void tcpChannelReady();
    bool isUdpConnectionAllowed();
    bool buildHostContext(const QStringList& addresses, quint32 port, quint32 encryptions,
        const QByteArray& public_key, const QByteArray& iv, quint32 request_id, PendingUdp* context);
    void readDirectUdpRequest(const proto::peer::DirectUdpRequest& request);
    void readStunUdpRequest(const proto::peer::StunUdpRequest& request);
    void readUpnpUdpRequest(const proto::peer::UpnpUdpRequest& request);
    bool setupUdpChannel(const PendingUdp& context, UdpChannelKeys* keys);
    void connectToUdp(const PendingUdp& context, qintptr socket = -1,
        const QString& external_address = QString(), quint16 external_port = 0);
    void startUdpHolePunching(const PendingUdp& context, const QString& stun_host, quint16 stun_port);
    void startUpnpListener(const PendingUdp& context);

    bool is_legacy_mode_ = false;
    ScopedQPointer<TcpChannel> tcp_channel_;
    ScopedQPointer<UdpChannel> udp_channel_;
    ScopedQPointer<RelayPeer> relay_peer_;
    ScopedQPointer<StunPeer> stun_peer_;
    ScopedQPointer<UpnpPortMapper> upnp_port_mapper_;

    std::optional<PendingUdp> pending_udp_context_;
    std::shared_ptr<SessionState> session_state_;

    enum class State { CREATED, STARTED, STOPPPED };
    State state_ = State::CREATED;

    bool udp_ready_ = false;
};

Q_DECLARE_METATYPE(Client::Status)

#endif // CLIENT_CLIENT_H
