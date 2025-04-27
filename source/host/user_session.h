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

#include "build/build_config.h"
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

#include <QTimer>

namespace base {
class ScopedTaskRunner;
} // namespace base

namespace host {

class UserSession final
    : public QObject,
      public base::IpcChannel::Listener,
      public DesktopSession::Delegate,
      public UnconfirmedClientSession::Delegate,
      public ClientSession::Delegate
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

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onUserSessionHostIdRequest(const QString& session_name) = 0;
        virtual void onUserSessionCredentialsChanged() = 0;
        virtual void onUserSessionDettached() = 0;
        virtual void onUserSessionFinished() = 0;
    };

    UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                base::SessionId session_id,
                std::unique_ptr<base::IpcChannel> channel,
                Delegate* delegate,
                QObject* parent = nullptr);
    ~UserSession() final;

    static const char* typeToString(Type type);
    static const char* stateToString(State state);

    void start(const proto::internal::RouterState& router_state);
    void restart(std::unique_ptr<base::IpcChannel> channel);

    Type type() const { return type_; }
    State state() const { return state_; }
    base::SessionId sessionId() const { return session_id_; }
    base::HostId hostId() const { return host_id_; }
    std::optional<QString> sessionName() const;
    base::User user() const;
    size_t clientsCount() const;
    bool isConnectedToUi() const { return channel_ != nullptr; }

    void onClientSession(std::unique_ptr<ClientSession> client_session);
    void onUserSessionEvent(base::win::SessionStatus status, base::SessionId session_id);
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onHostIdChanged(base::HostId host_id);
    void onSettingsChanged();

protected:
    // base::IpcChannel::Listener implementation.
    void onIpcDisconnected() final;
    void onIpcMessageReceived(const QByteArray& buffer) final;
    void onIpcMessageWritten() final;

    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() final;
    void onDesktopSessionStopped() final;
    void onScreenCaptured(const base::Frame* frame, const base::MouseCursor* cursor) final;
    void onScreenCaptureError(proto::VideoErrorCode error_code) final;
    void onAudioCaptured(const proto::AudioPacket& audio_packet) final;
    void onCursorPositionChanged(const proto::CursorPosition& cursor_position) final;
    void onScreenListChanged(const proto::ScreenList& list) final;
    void onScreenTypeChanged(const proto::ScreenType& type) final;
    void onClipboardEvent(const proto::ClipboardEvent& event) final;

    // UnconfirmedClientSession::Delegate implementation.
    void onUnconfirmedSessionAccept(uint32_t id) final;
    void onUnconfirmedSessionReject(uint32_t id) final;

    // ClientSession::Delegate implementation.
    void onClientSessionConfigured() final;
    void onClientSessionFinished() final;
    void onClientSessionVideoRecording(
        const QString& computer_name, const QString& user_name, bool started) final;
    void onClientSessionTextChat(uint32_t id, const proto::TextChat& text_chat) final;

private:
    void onSessionDettached(const base::Location& location);
    void sendConnectEvent(const ClientSession& client_session);
    void sendDisconnectEvent(uint32_t session_id);
    void updateCredentials(const base::Location& location);
    void sendCredentials(const base::Location& location);
    void killClientSession(uint32_t id);
    void sendRouterState(const base::Location& location);
    void sendHostIdRequest(const base::Location& location);
    void addNewClientSession(std::unique_ptr<ClientSession> client_session);
    void setState(const base::Location& location, State state);
    void onTextChatHasUser(const base::Location& location, bool has_user);
    void onTextChatSessionStarted(uint32_t id);
    void onTextChatSessionFinished(uint32_t id);
    void mergeAndSendConfiguration();

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::ScopedTaskRunner> scoped_task_runner_;
    std::unique_ptr<base::IpcChannel> channel_;

    Type type_;
    State state_ = State::DETTACHED;
    QTimer ui_attach_timer_;
    QTimer desktop_dettach_timer_;

    base::SessionId session_id_;
    proto::internal::RouterState router_state_;
    base::HostId host_id_ = base::kInvalidHostId;

    bool password_enabled_ = false;
    uint32_t password_characters_ = 0;
    int password_length_ = 0;
    std::chrono::milliseconds password_expire_interval_ { 0 };
    QTimer password_expire_timer_;
    QString one_time_password_;
    uint32_t one_time_sessions_ = 0;

    bool connection_confirmation_ = false;
    SystemSettings::NoUserAction no_user_action_ = SystemSettings::NoUserAction::ACCEPT;
    std::chrono::milliseconds auto_confirmation_interval_ { 0 };

    using ClientSessionPtr = std::unique_ptr<ClientSession>;
    using ClientSessionList = std::vector<ClientSessionPtr>;
    using UnconfirmedClientSessionPtr = std::unique_ptr<UnconfirmedClientSession>;
    using UnconfirmedClientSessionList = std::vector<UnconfirmedClientSessionPtr>;

    UnconfirmedClientSessionList pending_clients_;
    ClientSessionList desktop_clients_;
    ClientSessionList other_clients_;

    std::unique_ptr<DesktopSessionManager> desktop_session_;
    base::local_shared_ptr<DesktopSessionProxy> desktop_session_proxy_;

    Delegate* delegate_ = nullptr;

    proto::internal::UiToService incoming_message_;
    proto::internal::ServiceToUi outgoing_message_;

    DISALLOW_COPY_AND_ASSIGN(UserSession);
};

} // namespace host

#endif // HOST_USER_SESSION_H
