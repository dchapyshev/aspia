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

#ifndef HOST_USER_SESSION_H
#define HOST_USER_SESSION_H

#include <QList>
#include <QTimer>

#include "base/serialization.h"
#include "base/session_id.h"
#include "base/ipc/ipc_channel.h"
#include "base/peer/host_id.h"
#include "host/client.h"
#include "host/desktop_session_manager.h"
#include "host/system_settings.h"
#include "host/unconfirmed_client_session.h"
#include "proto/host_internal.h"

namespace host {

class UserSession final : public QObject
{
    Q_OBJECT

public:
    enum class Type
    {
        CONSOLE = 0,
        RDP = 1
    };
    Q_ENUM(Type)

    enum class State
    {
        STARTED = 0,
        DETTACHED = 1,
        FINISHED = 2
    };
    Q_ENUM(State)

    UserSession(base::SessionId session_id, base::IpcChannel* ipc_channel, QObject* parent = nullptr);
    ~UserSession() final;

    void start();
    void restart(base::IpcChannel* channel);

    Type type() const { return type_; }
    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }
    bool isConnectedToUi() const { return ipc_channel_ != nullptr; }

    void onClientSession(Client* client_session);
    void onUserSessionEvent(quint32 status, quint32 session_id);
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onUpdateCredentials(base::HostId host_id, const QString& password);
    void onSettingsChanged();

signals:
    void sig_routerStateRequested();
    void sig_credentialsRequested();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);
    void sig_finished();

private slots:
    void onClientSessionConfigured();
    void onClientSessionFinished();
    void onClientSessionVideoRecording(const QString& computer_name, const QString& user_name, bool started);
    void onClientSessionTextChat(quint32 id, const proto::text_chat::TextChat& text_chat);
    void onIpcDisconnected();
    void onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer);
    void onUnconfirmedSessionFinished(quint32 id, bool is_rejected);
    void onDesktopSessionStarted();
    void onDesktopSessionStopped();

private:
    void onSessionDettached(const base::Location& location);
    void sendConnectEvent(const Client& client_session);
    void sendDisconnectEvent(quint32 session_id);
    void stopClientSession(quint32 id);
    void addNewClientSession(Client* client_session);
    void setState(const base::Location& location, State state);
    void onTextChatHasUser(const base::Location& location, bool has_user);
    void onTextChatSessionStarted(quint32 id);
    void onTextChatSessionFinished(quint32 id);
    void mergeAndSendConfiguration();
    bool hasDesktopClients() const;
    void sendSessionMessage();

    base::IpcChannel* ipc_channel_ = nullptr;

    Type type_;
    State state_ = State::DETTACHED;
    QTimer* ui_attach_timer_ = nullptr;
    QTimer* desktop_dettach_timer_ = nullptr;

    base::SessionId session_id_;

    bool connect_confirmation_ = false;
    SystemSettings::NoUserAction no_user_action_ = SystemSettings::NoUserAction::ACCEPT;
    std::chrono::milliseconds auto_confirmation_interval_ { 0 };

    QList<UnconfirmedClientSession*> pending_clients_;
    QList<Client*> clients_;

    DesktopSessionManager* desktop_session_ = nullptr;

    base::Parser<proto::internal::UiToService> incoming_message_;
    base::Serializer<proto::internal::ServiceToUi> outgoing_message_;

    Q_DISABLE_COPY(UserSession)
};

} // namespace host

#endif // HOST_USER_SESSION_H
