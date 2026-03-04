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

#ifndef HOST_USER_SESSION_MANAGER_H
#define HOST_USER_SESSION_MANAGER_H

#include <QObject>
#include <QList>

#include "base/serialization.h"
#include "base/session_id.h"
#include "base/peer/host_id.h"
#include "proto/user.h"

namespace base {
class IpcChannel;
class IpcServer;
class Location;
} // namespace base

namespace host {

class UserSession final : public QObject
{
    Q_OBJECT

public:
    explicit UserSession(QObject* parent = nullptr);
    ~UserSession() final;

    enum class State
    {
        ATTACHED,
        ATTACHING,
        DETTACHED
    };
    Q_ENUM(State)

    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }

    void sendConnectEvent(quint32 client_id, proto::peer::SessionType session_type,
        const QString& computer_name, const QString& display_name);
    void sendDisconnectEvent(quint32 client_id);

public slots:
    bool start();
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onUpdateCredentials(base::HostId host_id, const QString& password);
    void onClientSwitchSession(base::SessionId session_id);
    void onClientConfirmation(const proto::user::ConfirmationRequest& request);
    void onClientStarted();
    void onClientFinished();
    void onClientChat(quint32 client_id, const proto::chat::Chat& chat);
    void onClientRecording(const QString& computer, const QString& user, bool started);

signals:
    void sig_attached();
    void sig_dettached();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);
    void sig_confirmationReply(quint32 request_id, bool accept);
    void sig_stopClient(quint32 client_id);
    void sig_chatMessage(const proto::chat::Chat& chat);
    void sig_lockMouseChanged(bool enable);
    void sig_lockKeyboardChanged(bool enable);
    void sig_pauseChanged(bool enable);

private slots:
    void onUserSessionEvent(quint32 status, quint32 session_id);
    void onIpcNewConnection();
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer);
    void onDettachTimeout();

private:
    void attach(const base::Location& location, base::SessionId session_id);
    void dettach(const base::Location& location);
    void sendMessage();

    base::IpcServer* ipc_server_ = nullptr;
    base::IpcChannel* ipc_channel_ = nullptr;
    QTimer* attach_timer_ = nullptr;
    QTimer* dettach_timer_ = nullptr;

    State state_ = State::DETTACHED;
    base::SessionId session_id_ = base::kInvalidSessionId;
    bool is_console_ = true;

    base::Parser<proto::user::UserToService> incoming_message_;
    base::Serializer<proto::user::ServiceToUser> outgoing_message_;

    Q_DISABLE_COPY_MOVE(UserSession)
};

} // namespace host

#endif // HOST_USER_SESSION_MANAGER_H
