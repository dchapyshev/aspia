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

#include "client/router.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "client/router_window_proxy.h"
#include "proto/router_common.pb.h"

namespace client {

Router::Router(std::shared_ptr<RouterWindowProxy> window_proxy,
               std::shared_ptr<base::TaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner),
      authenticator_(std::make_unique<base::ClientAuthenticator>(io_task_runner)),
      window_proxy_(std::move(window_proxy))
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

void Router::refreshSessionList()
{
    LOG(LS_INFO) << "Sending session list request";

    proto::AdminToRouter message;
    message.mutable_session_list_request()->set_dummy(1);
    channel_->send(base::serialize(message));
}

void Router::stopSession(int64_t session_id)
{
    LOG(LS_INFO) << "Sending disconnect request (session_id: " << session_id << ")";

    proto::AdminToRouter message;

    proto::SessionRequest* request = message.mutable_session_request();
    request->set_type(proto::SESSION_REQUEST_DISCONNECT);
    request->set_session_id(session_id);

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
    channel_->setOwnKeepAlive(true);
    channel_->setNoDelay(true);

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

    if (message.has_session_list())
    {
        LOG(LS_INFO) << "Session list received";

        window_proxy_->onSessionList(
            std::shared_ptr<proto::SessionList>(message.release_session_list()));
    }
    else if (message.has_session_result())
    {
        LOG(LS_INFO) << "Session result received with code: "
                     << message.session_result().error_code();

        window_proxy_->onSessionResult(
            std::shared_ptr<proto::SessionResult>(message.release_session_result()));
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

} // namespace client
