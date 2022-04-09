//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_USER_SESSION_AGENT_H
#define HOST_USER_SESSION_AGENT_H

#include "base/ipc/ipc_channel.h"
#include "proto/host_internal.pb.h"

namespace host {

class UserSessionAgentProxy;
class UserSessionWindowProxy;

class UserSessionAgent : public base::IpcChannel::Listener
{
public:
    enum class Status
    {
        CONNECTED_TO_SERVICE,
        DISCONNECTED_FROM_SERVICE,
        SERVICE_NOT_AVAILABLE
    };

    struct Client
    {
        explicit Client(const proto::internal::ConnectEvent& event)
            : id(event.id()),
              computer_name(event.computer_name()),
              session_type(event.session_type())
        {
            // Nothing
        }

        uint32_t id;
        std::string computer_name;
        proto::SessionType session_type;
    };

    using ClientList = std::vector<Client>;

    explicit UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy);
    ~UserSessionAgent() override;

    void start();

protected:
    // ipc::Channel::Listener implementation.
    void onDisconnected() override;
    void onMessageReceived(const base::ByteArray& buffer) override;

private:
    friend class UserSessionAgentProxy;

    void updateCredentials(proto::internal::CredentialsRequest::Type request_type);
    void killClient(uint32_t id);
    void connectConfirmation(uint32_t id, bool accept);
    void setVoiceChat(bool enable);
    void setMouseLock(bool enable);
    void setKeyboardLock(bool enable);
    void setPause(bool enable);
    void onTextChat(const proto::TextChat& text_chat);

    std::shared_ptr<UserSessionWindowProxy> window_proxy_;
    std::unique_ptr<base::IpcChannel> ipc_channel_;

    proto::internal::ServiceToUi incoming_message_;
    proto::internal::UiToService outgoing_message_;

    ClientList clients_;

    DISALLOW_COPY_AND_ASSIGN(UserSessionAgent);
};

} // namespace host

#endif // HOST_USER_SESSION_AGENT_H
