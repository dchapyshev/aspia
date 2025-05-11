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

#ifndef HOST_USER_SESSION_MANAGER_H
#define HOST_USER_SESSION_MANAGER_H

#include "build/build_config.h"
#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "base/win/session_status.h"
#include "host/user_session.h"

#include <QObject>

namespace host {

class UserSession;

class UserSessionManager final
    : public QObject,
      public UserSession::Delegate
{
    Q_OBJECT

public:
    explicit UserSessionManager(QObject* parent = nullptr);
    ~UserSessionManager() final;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onHostIdRequest(const QString& session_name) = 0;
        virtual void onResetHostId(base::HostId host_id) = 0;
        virtual void onUserListChanged() = 0;
    };

    bool start(Delegate* delegate);
    void onUserSessionEvent(base::SessionStatus status, base::SessionId session_id);
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onHostIdChanged(const QString& session_name, base::HostId host_id);
    void onSettingsChanged();
    void onClientSession(std::unique_ptr<ClientSession> client_session);
    std::unique_ptr<base::UserList> userList() const;

protected:
    // UserSession::Delegate implementation.
    void onUserSessionHostIdRequest(const QString& session_name) final;
    void onUserSessionCredentialsChanged() final;
    void onUserSessionDettached() final;
    void onUserSessionFinished() final;

private slots:
    void onIpcNewConnection();

private:
    void startSessionProcess(const base::Location& location, base::SessionId session_id);
    void addUserSession(const base::Location& location, base::SessionId session_id,
                        base::IpcChannel* channel);

    std::unique_ptr<base::IpcServer> ipc_server_;
    std::vector<std::unique_ptr<UserSession>> sessions_;
    Delegate* delegate_ = nullptr;

    proto::internal::RouterState router_state_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionManager);
};

} // namespace host

#endif // HOST_USER_SESSION_MANAGER_H
