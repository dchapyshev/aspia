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

#ifndef HOST__USER_SESSION_PROCESS_H
#define HOST__USER_SESSION_PROCESS_H

#include "base/macros_magic.h"
#include "ipc/ipc_listener.h"
#include "proto/host.pb.h"

namespace ipc {
class Channel;
} // namespace ipc

namespace host {

class UserSessionProcess : public ipc::Listener
{
public:
    UserSessionProcess();
    ~UserSessionProcess();

    enum class State
    {
        CONNECTED,
        DISCONNECTED
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onStateChanged() = 0;
        virtual void onClientListChanged() = 0;
        virtual void onCredentialsChanged() = 0;
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

    void start(Delegate* delegate);

    void updateCredentials(proto::CredentialsRequest::Type request_type);
    void killClient(const std::string& uuid);

    State state() const { return state_; }
    const proto::Credentials& credentials() const { return credentials_; }
    const ClientList& clients() const { return clients_; }

protected:
    // ipc::Listener implementation.
    void onConnected() override;
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    std::unique_ptr<ipc::Channel> ipc_channel_;

    State state_ = State::DISCONNECTED;
    proto::Credentials credentials_;
    ClientList clients_;

    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UserSessionProcess);
};

} // namespace host

#endif // HOST__USER_SESSION_PROCESS_H
