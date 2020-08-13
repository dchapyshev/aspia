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

#include "router/manager/router.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "proto/router_common.pb.h"
#include "router/manager/router_window_proxy.h"

namespace router {

Router::Router(std::shared_ptr<RouterWindowProxy> window_proxy,
               std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      window_proxy_(std::move(window_proxy)),
      authenticator_(std::make_unique<base::ClientAuthenticator>(io_task_runner))
{
    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setSessionType(proto::ROUTER_SESSION_ADMIN);
}

Router::~Router() = default;

void Router::setUserName(std::u16string_view user_name)
{
    authenticator_->setUserName(user_name);
}

void Router::setPassword(std::u16string_view password)
{
    authenticator_->setPassword(password);
}

void Router::connectToRouter(std::u16string_view address, uint16_t port)
{
    channel_ = std::make_unique<base::NetworkChannel>();
    channel_->setListener(this);
    channel_->connect(address, port);
}

void Router::refreshHostList()
{
    LOG(LS_INFO) << "Sending host list request";

    proto::AdminToRouter message;
    message.mutable_host_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::disconnectHost(base::HostId host_id)
{
    LOG(LS_INFO) << "Sending disconnect host request (host_id: " << host_id << ")";

    proto::AdminToRouter message;

    proto::HostRequest* request = message.mutable_host_request();
    request->set_type(proto::HOST_REQUEST_DISCONNECT);
    request->mutable_host()->set_host_id(host_id);

    channel_->send(base::serialize(message));
}

void Router::refreshRelayList()
{
    LOG(LS_INFO) << "Sending relay list request";

    proto::AdminToRouter message;
    message.mutable_relay_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::refreshUserList()
{
    LOG(LS_INFO) << "Sending user list request";

    proto::AdminToRouter message;
    message.mutable_user_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::addUser(const proto::User& user)
{
    LOG(LS_INFO) << "Sending user add request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_ADD);
    request->mutable_user()->CopyFrom(user);

    channel_->send(base::serialize(message));
}

void Router::modifyUser(const proto::User& user)
{
    LOG(LS_INFO) << "Sending user modify request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_MODIFY);
    request->mutable_user()->CopyFrom(user);

    channel_->send(base::serialize(message));
}

void Router::deleteUser(int64_t entry_id)
{
    LOG(LS_INFO) << "Sending user delete request (entry_id: " << entry_id << ")";

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_DELETE);
    request->mutable_user()->set_entry_id(entry_id);

    channel_->send(base::serialize(message));
}

void Router::onConnected()
{
    authenticator_->start(std::move(channel_),
                          [this](base::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == base::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            window_proxy_->onConnected(authenticator_->peerVersion());

            // Now the session will receive incoming messages.
            channel_->resume();
        }
        else
        {
            window_proxy_->onAccessDenied(error_code);
        }

        // Authenticator is no longer needed.
        io_task_runner_->deleteSoon(std::move(authenticator_));
    });
}

void Router::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    window_proxy_->onDisconnected(error_code);
}

void Router::onMessageReceived(const base::ByteArray& buffer)
{
    proto::RouterToAdmin message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Failed to read the message from the router";
        return;
    }

    if (message.has_host_list())
    {
        LOG(LS_INFO) << "Host list received";

        window_proxy_->onHostList(
            std::shared_ptr<proto::HostList>(message.release_host_list()));
    }
    else if (message.has_host_result())
    {
        LOG(LS_INFO) << "Host result received with code: " << message.host_result().error_code();

        window_proxy_->onHostResult(
            std::shared_ptr<proto::HostResult>(message.release_host_result()));
    }
    else if (message.has_relay_list())
    {
        LOG(LS_INFO) << "Relay list received";

        window_proxy_->onRelayList(
            std::shared_ptr<proto::RelayList>(message.release_relay_list()));
    }
    else if (message.has_user_list())
    {
        LOG(LS_INFO) << "User list received";

        window_proxy_->onUserList(
            std::shared_ptr<proto::UserList>(message.release_user_list()));
    }
    else if (message.has_user_result())
    {
        LOG(LS_INFO) << "User result received with code: " << message.user_result().error_code();

        window_proxy_->onUserResult(
            std::shared_ptr<proto::UserResult>(message.release_user_result()));
    }
    else
    {
        LOG(LS_ERROR) << "Unknown message type from the router";
    }
}

void Router::onMessageWritten(size_t /* pending */)
{
    // Not used.
}

} // namespace router
