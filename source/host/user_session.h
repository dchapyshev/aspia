//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__USER_SESSION_H
#define HOST__USER_SESSION_H

#include "base/macros_magic.h"
#include "base/win/session_id.h"
#include "host/client_session.h"
#include "host/desktop_session_manager.h"
#include "host/user.h"
#include "ipc/ipc_listener.h"
#include "proto/host_internal.pb.h"

#include <memory>

namespace ipc {
class Channel;
class ChannelProxy;
} // namespace ipc

namespace host {

class UserSession
    : public ipc::Listener,
      public DesktopSession::Delegate,
      public ClientSession::Delegate
{
public:
    enum class Type
    {
        CONSOLE,
        RDP
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onUserSessionStarted() = 0;
        virtual void onUserSessionFinished() = 0;
    };

    UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                std::unique_ptr<ipc::Channel> ipc_channel);
    ~UserSession();

    void start(Delegate* delegate);
    void restart(std::unique_ptr<ipc::Channel> ipc_channel);

    Type type() const;
    base::win::SessionId sessionId() const;
    User user() const;

    void addNewSession(std::unique_ptr<ClientSession> client_session);
    void setSessionEvent(base::win::SessionStatus status, base::win::SessionId session_id);

protected:
    // ipc::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() override;
    void onDesktopSessionStopped() override;
    void onScreenCaptured(const desktop::Frame& frame) override;
    void onCursorCaptured(std::shared_ptr<desktop::MouseCursor> mouse_cursor) override;
    void onScreenListChanged(const proto::ScreenList& list) override;
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

    // ClientSession::Delegate implementation.
    void onClientSessionFinished() override;

private:
    void sendConnectEvent(const ClientSession& client_session);
    void sendDisconnectEvent(const std::string& session_id);
    void updateCredentials();
    void sendCredentials();
    void killClientSession(std::string_view id);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<ipc::Channel> ipc_channel_;

    Type type_;
    base::win::SessionId session_id_;
    std::string username_;
    std::string password_;

    using ClientSessionPtr = std::unique_ptr<ClientSession>;
    using ClientSessionList = std::vector<ClientSessionPtr>;

    ClientSessionList desktop_clients_;
    ClientSessionList file_transfer_clients_;

    std::unique_ptr<DesktopSessionManager> desktop_session_;
    std::shared_ptr<DesktopSessionProxy> desktop_session_proxy_;

    proto::internal::UiToService incoming_message_;
    proto::internal::ServiceToUi outgoing_message_;

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UserSession);
};

} // namespace host

#endif // HOST__USER_SESSION_H
