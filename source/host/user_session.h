//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <list>
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
    UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                std::unique_ptr<ipc::Channel> ipc_channel);
    ~UserSession();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onUserSessionStarted() = 0;
        virtual void onUserSessionFinished() = 0;
    };

    void start(Delegate* delegate);

    base::win::SessionId sessionId() const;
    User user() const;

    void addNewSession(std::unique_ptr<ClientSession> client_session);

protected:
    // ipc::Listener implementation.
    void onConnected() override;
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

    // DesktopSession::Delegate implementation.
    void onDesktopSessionStarted() override;
    void onDesktopSessionStopped() override;
    void onScreenCaptured(std::unique_ptr<desktop::Frame> frame) override;
    void onScreenListChanged(const proto::ScreenList& list) override;
    void onClipboardEvent(const proto::ClipboardEvent& event) override;

    // ClientSession::Delegate implementation.
    void onClientSessionFinished() override;

private:
    void updateCredentials();
    void sendCredentials();
    void killClientSession(const std::string& id);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<ipc::Channel> ipc_channel_;
    std::shared_ptr<ipc::ChannelProxy> ipc_channel_proxy_;

    base::win::SessionId session_id_ = base::win::kInvalidSessionId;
    std::string username_;
    std::string password_;

    std::list<std::unique_ptr<ClientSession>> clients_;
    std::unique_ptr<DesktopSessionManager> desktop_session_;
    std::shared_ptr<DesktopSessionProxy> desktop_session_proxy_;

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UserSession);
};

} // namespace host

#endif // HOST__USER_SESSION_H
