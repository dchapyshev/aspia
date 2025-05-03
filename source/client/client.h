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

#ifndef CLIENT_CLIENT_H
#define CLIENT_CLIENT_H

#include "client/client_config.h"
#include "client/client_session_state.h"
#include "client/router_controller.h"
#include "base/net/tcp_channel.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariant>

namespace base {
class ClientAuthenticator;
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
        ACCESS_DENIED
    };

    static const char* statusToString(Status status);

signals:
    void sig_statusChanged(Client::Status status, const QVariant& data = QVariant());
    void sig_showSessionWindow();

protected:
    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onSessionStarted() = 0;
    virtual void onSessionMessageReceived(quint8 channel_id, const QByteArray& buffer) = 0;
    virtual void onSessionMessageWritten(quint8 channel_id, size_t pending) = 0;

    // Sends outgoing message.
    void sendMessage(quint8 channel_id, const google::protobuf::MessageLite& message);

    // Methods for obtaining network metrics.
    qint64 totalRx() const;
    qint64 totalTx() const;
    int speedRx();
    int speedTx();

private slots:
    void onTcpConnected();
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer);
    void onTcpMessageWritten(quint8 channel_id, size_t pending);
    void onRouterConnected(const QVersionNumber& router_version);
    void onHostAwaiting();
    void onHostConnected();
    void onRouterErrorOccurred(const client::RouterController::Error& error);

private:
    void startAuthentication();
    void delayedReconnect();

    QPointer<QTimer> timeout_timer_;
    QPointer<QTimer> reconnect_timer_;
    QPointer<RouterController> router_controller_;
    QPointer<base::TcpChannel> channel_;
    QPointer<base::ClientAuthenticator> authenticator_;

    std::shared_ptr<SessionState> session_state_;

    enum class State { CREATED, STARTED, STOPPPED };
    State state_ = State::CREATED;

    bool is_connected_to_router_ = false;
};

} // namespace client

Q_DECLARE_METATYPE(client::Client::Status)

#endif // CLIENT_CLIENT_H
