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

#ifndef HOST_USER_SESSION_AGENT_PROXY_H
#define HOST_USER_SESSION_AGENT_PROXY_H

#include "host/user_session_agent.h"

namespace base {
class TaskRunner;
} // namespace base

namespace host {

class UserSessionAgentProxy
{
public:
    UserSessionAgentProxy(std::shared_ptr<base::TaskRunner> io_task_runner,
                          std::unique_ptr<UserSessionAgent> agent);
    ~UserSessionAgentProxy();

    void start();
    void stop();

    void updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void setOneTimeSessions(quint32 sessions);
    void killClient(quint32 id);
    void connectConfirmation(quint32 id, bool accept);
    void setVoiceChat(bool enable);
    void setMouseLock(bool enable);
    void setKeyboardLock(bool enable);
    void setPause(bool enable);
    void onTextChat(const proto::TextChat& text_chat);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionAgentProxy);
};

} // namespace host

#endif // HOST_USER_SESSION_AGENT_PROXY_H
