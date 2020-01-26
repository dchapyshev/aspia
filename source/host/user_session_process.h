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

#ifndef HOST__USER_SESSION_PROCESS_H
#define HOST__USER_SESSION_PROCESS_H

#include "base/macros_magic.h"
#include "base/threading/thread.h"
#include "ipc/ipc_listener.h"
#include "proto/host.pb.h"

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class UserSessionProcessProxy;
class UserSessionWindowProxy;

class UserSessionProcess
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
        explicit Client(const proto::ConnectEvent& event)
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

    explicit UserSessionProcess(std::shared_ptr<UserSessionWindowProxy> window_proxy);
    ~UserSessionProcess();

    void start();

    std::shared_ptr<UserSessionProcessProxy> processProxy() const { return process_proxy_; }

protected:
    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // ipc::Listener implementation.
    void onConnected() override;
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    friend class UserSessionProcessProxy;

    void updateCredentials(proto::CredentialsRequest::Type request_type);
    void killClient(const std::string& uuid);

    base::Thread io_thread_;

    std::shared_ptr<UserSessionProcessProxy> process_proxy_;
    std::shared_ptr<UserSessionWindowProxy> window_proxy_;
    std::unique_ptr<ipc::Channel> ipc_channel_;

    ClientList clients_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionProcess);
};

} // namespace host

#endif // HOST__USER_SESSION_PROCESS_H
