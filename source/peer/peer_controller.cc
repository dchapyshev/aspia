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

#include "peer/peer_controller.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "peer/client_authenticator.h"
#include "proto/router.pb.h"

namespace peer {

namespace {

const std::chrono::seconds kReconnectTimeout{ 15 };

} // namespace

PeerController::PeerController(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(task_runner),
      reconnect_timer_(task_runner)
{
    // TODO
}

PeerController::~PeerController()
{
    // TODO
}

void PeerController::start(const RouterInfo& router_info, Delegate* delegate)
{
    router_info_ = router_info;
    delegate_ = delegate;

    if (!delegate_)
    {
        LOG(LS_ERROR) << "Invalid parameters";
        return;
    }

    LOG(LS_INFO) << "Starting peer controller for router: "
                 << router_info_.address << ":" << router_info_.port;

    connectToRouter();
}

void PeerController::connectTo(peer::PeerId peer_id)
{
    // TODO
}

void PeerController::onConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    static const std::chrono::seconds kKeepAliveTime{ 30 };
    static const std::chrono::seconds kKeepAliveInterval{ 3 };

    channel_->setKeepAlive(true, kKeepAliveTime, kKeepAliveInterval);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<peer::ClientAuthenticator>(task_runner_);

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(router_info_.public_key);
    authenticator_->setSessionType(proto::ROUTER_SESSION_ANONIMOUS_PEER);

    authenticator_->start(std::move(channel_),
                          [this](peer::ClientAuthenticator::ErrorCode error_code)
    {
        if (error_code == peer::ClientAuthenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            LOG(LS_INFO) << "Router connected. Receiving peer ID...";

            delegate_->onRouterConnected();

            proto::PeerToRouter message;
            proto::PeerIdRequest* peer_id_request = message.mutable_peer_id_request();

            if (router_info_.peer_key.empty())
            {
                peer_id_request->set_type(proto::PeerIdRequest::NEW_ID);
            }
            else
            {
                peer_id_request->set_type(proto::PeerIdRequest::EXISTING_ID);
                peer_id_request->set_key(base::toStdString(router_info_.peer_key));
            }

            // Now the session will receive incoming messages.
            channel_->resume();

            // Send peer ID request.
            channel_->send(base::serialize(message));
        }
        else
        {
            LOG(LS_WARNING) << "Authentication failed: "
                            << ClientAuthenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        task_runner_->deleteSoon(std::move(authenticator_));
    });
}

void PeerController::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::NetworkChannel::errorToString(error_code) << ")";

    delegate_->onPeerIdAssigned(kInvalidPeerId, base::ByteArray());
    delegate_->onRouterDisconnected(error_code);
    peer_id_ = kInvalidPeerId;

    delayedConnectToRouter();
}

void PeerController::onMessageReceived(const base::ByteArray& buffer)
{
    proto::RouterToPeer message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from router";
        return;
    }

    if (message.has_peer_id_response())
    {
        if (peer_id_ != kInvalidPeerId)
        {
            LOG(LS_ERROR) << "Peer ID already assigned";
            return;
        }

        const proto::PeerIdResponse& peer_id_response = message.peer_id_response();
        if (peer_id_response.peer_id() == kInvalidPeerId)
        {
            LOG(LS_ERROR) << "Invalid peer ID received";
            return;
        }

        LOG(LS_INFO) << "Peer ID received: " << peer_id_response.peer_id();

        base::ByteArray peer_key = base::fromStdString(peer_id_response.key());
        if (!peer_key.empty())
            router_info_.peer_key = base::fromStdString(peer_id_response.key());
        peer_id_ = peer_id_response.peer_id();

        delegate_->onPeerIdAssigned(peer_id_, router_info_.peer_key);
    }
    else
    {
        if (peer_id_ == kInvalidPeerId)
        {
            LOG(LS_ERROR) << "Request could not be processed (peer ID not received yet)";
            return;
        }

        if (message.has_connection_request())
        {
            LOG(LS_INFO) << "CONNECTION REQUEST";
        }
        else if (message.has_connection_response())
        {
            LOG(LS_INFO) << "CONNECTION RESPONSE";
        }
        else if (message.has_connection_offer())
        {
            LOG(LS_INFO) << "CONNECTION OFFER";
        }
        else
        {
            LOG(LS_WARNING) << "Unhandled message from router";
        }
    }
}

void PeerController::onMessageWritten(size_t /* pending */)
{
    // Nothing
}

void PeerController::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    channel_ = std::make_unique<base::NetworkChannel>();
    channel_->setListener(this);
    channel_->connect(router_info_.address, router_info_.port);
}

void PeerController::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_.start(kReconnectTimeout, std::bind(&PeerController::connectToRouter, this));
}

} // namespace peer
