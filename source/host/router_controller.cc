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

#include "host/router_controller.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/peer/client_authenticator.h"
#include "base/strings/unicode.h"
#include "proto/router_host.pb.h"

namespace host {

namespace {

const std::chrono::seconds kReconnectTimeout{ 10 };

} // namespace

RouterController::RouterController(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(task_runner),
      peer_manager_(std::make_unique<base::RelayPeerManager>(task_runner, this)),
      reconnect_timer_(task_runner)
{
    // TODO
}

RouterController::~RouterController() = default;

void RouterController::start(const RouterInfo& router_info, Delegate* delegate)
{
    router_info_ = router_info;
    delegate_ = delegate;

    if (!delegate_)
    {
        LOG(LS_ERROR) << "Invalid parameters";
        return;
    }

    LOG(LS_INFO) << "Starting host controller for router: "
                 << router_info_.address << ":" << router_info_.port;

    connectToRouter();
}

void RouterController::onConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    static const std::chrono::seconds kKeepAliveTime{ 30 };
    static const std::chrono::seconds kKeepAliveInterval{ 3 };

    channel_->setKeepAlive(true, kKeepAliveTime, kKeepAliveInterval);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>(task_runner_);

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(router_info_.public_key);
    authenticator_->setSessionType(proto::ROUTER_SESSION_HOST);

    authenticator_->start(std::move(channel_),
                          [this](base::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == base::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            LOG(LS_INFO) << "Router connected. Receiving host ID...";

            routerStateChanged(proto::internal::RouterState::CONNECTED);

            proto::HostToRouter message;
            proto::HostIdRequest* host_id_request = message.mutable_host_id_request();

            if (router_info_.host_key.empty())
            {
                host_id_request->set_type(proto::HostIdRequest::NEW_ID);
            }
            else
            {
                host_id_request->set_type(proto::HostIdRequest::EXISTING_ID);
                host_id_request->set_key(base::toStdString(router_info_.host_key));
            }

            // Now the session will receive incoming messages.
            channel_->resume();

            // Send peer ID request.
            channel_->send(base::serialize(message));
        }
        else
        {
            LOG(LS_WARNING) << "Authentication failed: "
                            << base::ClientAuthenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        task_runner_->deleteSoon(std::move(authenticator_));
    });
}

void RouterController::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::NetworkChannel::errorToString(error_code) << ")";

    host_id_ = base::kInvalidHostId;
    routerStateChanged(proto::internal::RouterState::FAILED);

    if (delegate_)
        delegate_->onHostIdAssigned(base::kInvalidHostId, base::ByteArray());

    delayedConnectToRouter();
}

void RouterController::onMessageReceived(const base::ByteArray& buffer)
{
    proto::RouterToHost message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from router";
        return;
    }

    if (message.has_host_id_response())
    {
        if (host_id_ != base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Host ID already assigned";
            return;
        }

        const proto::HostIdResponse& host_id_response = message.host_id_response();
        if (host_id_response.host_id() == base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Invalid host ID received";
            return;
        }

        LOG(LS_INFO) << "Host ID received: " << host_id_response.host_id();

        base::ByteArray peer_key = base::fromStdString(host_id_response.key());
        if (!peer_key.empty())
            router_info_.host_key = base::fromStdString(host_id_response.key());
        host_id_ = host_id_response.host_id();

        delegate_->onHostIdAssigned(host_id_, router_info_.host_key);
    }
    else
    {
        if (host_id_ == base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Request could not be processed (host ID not received yet)";
            return;
        }

        if (message.has_connection_offer())
        {
            const proto::ConnectionOffer& connection_offer = message.connection_offer();

            if (connection_offer.error_code() == proto::ConnectionOffer::SUCCESS)
                peer_manager_->addConnectionOffer(connection_offer.relay());
        }
        else
        {
            LOG(LS_WARNING) << "Unhandled message from router";
        }
    }
}

void RouterController::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void RouterController::onNewPeerConnected(std::unique_ptr<base::NetworkChannel> channel)
{
    if (delegate_)
        delegate_->onClientConnected(std::move(channel));
}

void RouterController::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    routerStateChanged(proto::internal::RouterState::CONNECTING);

    channel_ = std::make_unique<base::NetworkChannel>();
    channel_->setListener(this);
    channel_->connect(router_info_.address, router_info_.port);
}

void RouterController::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_.start(kReconnectTimeout, std::bind(&RouterController::connectToRouter, this));
}

void RouterController::routerStateChanged(proto::internal::RouterState::State state)
{
    if (!delegate_)
        return;

    proto::internal::RouterState router_state;
    router_state.set_state(state);

    router_state.set_host_name(base::utf8FromUtf16(router_info_.address));
    router_state.set_host_port(router_info_.port);

    delegate_->onRouterStateChanged(router_state);
}

} // namespace host
