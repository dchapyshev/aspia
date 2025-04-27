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

#include "base/version.h"
#include "client/client_config.h"
#include "client/client_session_state.h"
#include "client/router_controller.h"
#include "base/net/tcp_channel.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace base {
class ClientAuthenticator;
class TaskRunner;
} // namespace base

namespace client {

class StatusWindow;
class StatusWindowProxy;

class Client
    : public QObject,
      public RouterController::Delegate,
      public base::TcpChannel::Listener
{
    Q_OBJECT

public:
    explicit Client(QObject* parent);
    virtual ~Client() override;

    // Starts a session.
    void start();

    // Stops a session.
    void stop();

    // Sets the implementation of the status window.
    // The method must be called before calling method start().
    void setStatusWindow(std::shared_ptr<StatusWindowProxy> status_window_proxy);

    // Sets an instance of a class that stores session state.
    // The method must be called before calling method start().
    void setSessionState(std::shared_ptr<SessionState> session_state);

    std::shared_ptr<SessionState> sessionState() { return session_state_; }

protected:
    // Indicates that the session is started.
    // When calling this method, the client implementation should display a session window.
    virtual void onSessionStarted() = 0;
    virtual void onSessionMessageReceived(uint8_t channel_id, const QByteArray& buffer) = 0;
    virtual void onSessionMessageWritten(uint8_t channel_id, size_t pending) = 0;

    // Sends outgoing message.
    void sendMessage(uint8_t channel_id, const google::protobuf::MessageLite& message);

    // Methods for obtaining network metrics.
    int64_t totalRx() const;
    int64_t totalTx() const;
    int speedRx();
    int speedTx();

    // base::TcpChannel::Listener implementation.
    void onTcpConnected() final;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) final;
    void onTcpMessageReceived(uint8_t channel_id, const QByteArray& buffer) final;
    void onTcpMessageWritten(uint8_t channel_id, size_t pending) final;

    // RouterController::Delegate implementation.
    void onRouterConnected(const base::Version& router_version) final;
    void onHostAwaiting() final;
    void onHostConnected(std::unique_ptr<base::TcpChannel> channel) final;
    void onErrorOccurred(const RouterController::Error& error) final;

private:
    void startAuthentication();
    void delayedReconnectToRouter();
    void delayedReconnectToHost();

    QPointer<QTimer> timeout_timer_;
    QPointer<QTimer> reconnect_timer_;
    std::unique_ptr<RouterController> router_controller_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::shared_ptr<StatusWindowProxy> status_window_proxy_;

    std::shared_ptr<SessionState> session_state_;

    enum class State { CREATED, STARTED, STOPPPED };
    State state_ = State::CREATED;

    bool is_connected_to_router_ = false;
};

} // namespace client

#endif // CLIENT_CLIENT_H
