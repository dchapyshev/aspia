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

#ifndef HOST_USER_SESSION_H
#define HOST_USER_SESSION_H

#include "base/session_id.h"
#include "base/ipc/ipc_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "base/win/session_status.h"
#include "host/client_session.h"
#include "host/desktop_session_manager.h"
#include "host/system_settings.h"
#include "host/unconfirmed_client_session.h"
#include "proto/host_internal.pb.h"

#include <QList>
#include <QTimer>

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

    enum class State
    {
        STARTED = 0,
        DETTACHED = 1,
        FINISHED = 2
    };

    UserSession(base::SessionId session_id, base::IpcChannel* channel, QObject* parent = nullptr);
    ~UserSession() final;

    static const char* typeToString(Type type);
    static const char* stateToString(State state);

    void start(const proto::internal::RouterState& router_state);
    void restart(base::IpcChannel* channel);

    Type type() const { return type_; }
    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }
    base::HostId hostId() const { return host_id_; }
    base::User user() const;
    size_t clientsCount() const;
    bool isConnectedToUi() const { return channel_ != nullptr; }

    void onClientSession(ClientSession* client_session);
    void onUserSessionEvent(base::SessionStatus status, base::SessionId session_id);
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onHostIdChanged(base::HostId host_id);
    void onSettingsChanged();

signals:
    void sig_userSessionHostIdRequest();
    void sig_userSessionCredentialsChanged();
    void sig_userSessionDettached();
    void sig_userSessionFinished();

private slots:
    void onClientSessionConfigured();
    void onClientSessionFinished();
    void onClientSessionVideoRecording(const QString& computer_name, const QString& user_name, bool started);
    void onClientSessionTextChat(quint32 id, const proto::TextChat& text_chat);
    void onIpcDisconnected();
    void onIpcMessageReceived(const QByteArray& buffer);
    void onUnconfirmedSessionFinished(quint32 id, bool is_rejected);
    void onDesktopSessionStarted();
    void onDesktopSessionStopped();
    void onScreenCaptured(const base::Frame* frame, const base::MouseCursor* cursor);
    void onScreenCaptureError(proto::VideoErrorCode error_code);
    void onAudioCaptured(const proto::AudioPacket& audio_packet);
    void onCursorPositionChanged(const proto::CursorPosition& cursor_position);
    void onScreenListChanged(const proto::ScreenList& list);
    void onScreenTypeChanged(const proto::ScreenType& type);
    void onClipboardEvent(const proto::ClipboardEvent& event);

private:
    void onSessionDettached(const base::Location& location);
    void sendConnectEvent(const ClientSession& client_session);
    void sendDisconnectEvent(quint32 session_id);
    void updateCredentials(const base::Location& location);
    void sendCredentials(const base::Location& location);
    void killClientSession(quint32 id);
    void sendRouterState(const base::Location& location);
    void sendHostIdRequest(const base::Location& location);
    void addNewClientSession(ClientSession* client_session);
    void setState(const base::Location& location, State state);
    void onTextChatHasUser(const base::Location& location, bool has_user);
    void onTextChatSessionStarted(quint32 id);
    void onTextChatSessionFinished(quint32 id);
    void mergeAndSendConfiguration();

    base::IpcChannel* channel_ = nullptr;

    Type type_;
    State state_ = State::DETTACHED;
    QTimer* ui_attach_timer_ = nullptr;
    QTimer* desktop_dettach_timer_ = nullptr;

    base::SessionId session_id_;
    proto::internal::RouterState router_state_;
    base::HostId host_id_ = base::kInvalidHostId;

    bool password_enabled_ = false;
    quint32 password_characters_ = 0;
    int password_length_ = 0;
    std::chrono::milliseconds password_expire_interval_ { 0 };
    QTimer* password_expire_timer_ = nullptr;
    QString one_time_password_;
    quint32 one_time_sessions_ = 0;

    bool connection_confirmation_ = false;
    SystemSettings::NoUserAction no_user_action_ = SystemSettings::NoUserAction::ACCEPT;
    std::chrono::milliseconds auto_confirmation_interval_ { 0 };

    using ClientSessionList = QList<ClientSession*>;
    using UnconfirmedClientSessionList = QList<UnconfirmedClientSession*>;

    UnconfirmedClientSessionList pending_clients_;
    ClientSessionList desktop_clients_;
    ClientSessionList other_clients_;

    DesktopSessionManager* desktop_session_ = nullptr;

    proto::internal::UiToService incoming_message_;
    proto::internal::ServiceToUi outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(UserSession);
};

} // namespace host

#endif // HOST_USER_SESSION_H
