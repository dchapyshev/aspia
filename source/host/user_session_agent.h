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

#ifndef HOST__USER_SESSION_AGENT_H
#define HOST__USER_SESSION_AGENT_H

#include "base/macros_magic.h"
#include "base/threading/thread.h"
#include "ipc/ipc_listener.h"
#include "proto/host_internal.pb.h"

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class UserSessionAgentProxy;
class UserSessionWindowProxy;

class UserSessionAgent
    : public base::Thread::Delegate,
      public ipc::Listener
{
public:
    enum class State
    {
        CONNECTED,
        DISCONNECTED
    };

    struct Client
    {
        explicit Client(const proto::internal::ConnectEvent& event)
            : uuid(event.uuid()),
              address(event.remote_address()),
              username(event.username()),
              session_type(event.session_type())
        {
            // Nothing
        }

        std::string uuid;
        std::string address;
        std::string username;
        proto::SessionType session_type;
    };

    using ClientList = std::vector<Client>;

    explicit UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy);
    ~UserSessionAgent();

    void start();

    std::shared_ptr<UserSessionAgentProxy> agentProxy() const { return agent_proxy_; }

protected:
    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // ipc::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    friend class UserSessionAgentProxy;

    void updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void killClient(const std::string& uuid);

    base::Thread io_thread_;

    std::shared_ptr<UserSessionAgentProxy> agent_proxy_;
    std::shared_ptr<UserSessionWindowProxy> window_proxy_;
    std::unique_ptr<ipc::Channel> ipc_channel_;

    proto::internal::ServiceToUi incoming_message_;
    proto::internal::UiToService outgoing_message_;

    ClientList clients_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionAgent);
};

} // namespace host

#endif // HOST__USER_SESSION_AGENT_H
