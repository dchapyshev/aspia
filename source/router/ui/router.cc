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

#include "router/ui/router.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "router/ui/router_window_proxy.h"

namespace router {

Router::Router(std::shared_ptr<RouterWindowProxy> window_proxy,
               std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)),
      window_proxy_(std::move(window_proxy)),
      channel_(std::make_unique<net::Channel>()),
      authenticator_(std::make_unique<net::ClientAuthenticator>())
{
    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setSessionType(proto::ROUTER_SESSION_MANAGER);
}

Router::~Router() = default;

void Router::setPublicKey(const base::ByteArray& public_key)
{
    authenticator_->setPeerPublicKey(public_key);
}

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
    channel_->setListener(this);
    channel_->connect(address, port);
}

void Router::refreshPeerList()
{
    proto::ManagerToRouter message;
    message.mutable_peer_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::disconnectPeer(uint64_t peer_id)
{
    proto::ManagerToRouter message;

    proto::PeerRequest* request = message.mutable_peer_request();
    request->set_type(proto::PEER_REQUEST_DISCONNECT);
    request->mutable_peer()->set_peer_id(peer_id);

    channel_->send(base::serialize(message));
}

void Router::refreshProxyList()
{
    proto::ManagerToRouter message;
    message.mutable_proxy_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::refreshUserList()
{
    proto::ManagerToRouter message;
    message.mutable_user_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::addUser(const proto::User& user)
{
    proto::ManagerToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_ADD);
    request->mutable_user()->CopyFrom(user);

    channel_->send(base::serialize(message));
}

void Router::modifyUser(const proto::User& user)
{
    proto::ManagerToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_MODIFY);
    request->mutable_user()->CopyFrom(user);

    channel_->send(base::serialize(message));
}

void Router::deleteUser(uint64_t entry_id)
{
    proto::ManagerToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_DELETE);
    request->mutable_user()->set_entry_id(entry_id);

    channel_->send(base::serialize(message));
}

void Router::onConnected()
{
    authenticator_->start(std::move(channel_),
                          [this](net::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == net::ClientAuthenticator::ErrorCode::SUCCESS)
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

void Router::onDisconnected(net::Channel::ErrorCode error_code)
{
    window_proxy_->onDisconnected(error_code);
}

void Router::onMessageReceived(const base::ByteArray& buffer)
{
    proto::RouterToManager message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Failed to read the message from the router";
        return;
    }

    if (message.has_peer_list())
    {
        window_proxy_->onPeerList(
            std::shared_ptr<proto::PeerList>(message.release_peer_list()));
    }
    else if (message.has_peer_result())
    {
        window_proxy_->onPeerResult(
            std::shared_ptr<proto::PeerResult>(message.release_peer_result()));
    }
    else if (message.has_proxy_list())
    {
        window_proxy_->onProxyList(
            std::shared_ptr<proto::ProxyList>(message.release_proxy_list()));
    }
    else if (message.has_user_list())
    {
        window_proxy_->onUserList(
            std::shared_ptr<proto::UserList>(message.release_user_list()));
    }
    else if (message.has_user_result())
    {
        window_proxy_->onUserResult(
            std::shared_ptr<proto::UserResult>(message.release_user_result()));
    }
    else
    {
        LOG(LS_ERROR) << "Unknown message type from the router";
    }
}

void Router::onMessageWritten()
{
    // Not used.
}

} // namespace router
