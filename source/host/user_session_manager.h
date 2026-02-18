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

#include "base/session_id.h"
#include "base/ipc/ipc_server.h"
#include "host/user_session.h"

namespace host {

class UserSessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit UserSessionManager(QObject* parent = nullptr);
    ~UserSessionManager() final;

    bool start();
    void onRouterStateChanged(const proto::internal::RouterState& router_state);
    void onUpdateCredentials(base::HostId host_id, const QString& password);
    void onSettingsChanged();
    void onClientSession(Client* client_session);

signals:
    void sig_routerStateRequested();
    void sig_credentialsRequested();
    void sig_changeOneTimePassword();
    void sig_changeOneTimeSessions(quint32 sessions);

private slots:
    void onUserSessionEvent(quint32 status, quint32 session_id);
    void onUserSessionFinished();
    void onIpcNewConnection();

private:
    void startSessionProcess(const base::Location& location, base::SessionId session_id);
    void addUserSession(const base::Location& location, base::SessionId session_id,
                        base::IpcChannel* channel);

    base::IpcServer* ipc_server_ = nullptr;
    QList<UserSession*> sessions_;

    Q_DISABLE_COPY(UserSessionManager)
};

} // namespace host

#endif // HOST_USER_SESSION_MANAGER_H
