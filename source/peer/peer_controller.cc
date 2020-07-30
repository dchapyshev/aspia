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
#include "proto/router.pb.h"

namespace peer {

namespace {

const std::chrono::seconds kReconnectTimeout{ 10 };

} // namespace

PeerController::PeerController(std::shared_ptr<base::TaskRunner> task_runner)
    : reconnect_timer_(std::move(task_runner))
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

    connectToRouter();
}

void PeerController::connectTo(peer::PeerId peer_id)
{
    // TODO
}

void PeerController::onConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";
    delegate_->onRouterConnected();

    LOG(LS_INFO) << "Receiving peer ID...";

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

    channel_->send(base::serialize(message));
}

void PeerController::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::NetworkChannel::errorToString(error_code) << ")";

    delegate_->onRouterDisconnected(error_code);
    peer_id_ = kInvalidPeerId;

    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_.start(kReconnectTimeout, [this]()
    {
        connectToRouter();
    });
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
        peer_id_ = peer_id_response.peer_id();
        delegate_->onPeerIdAssigned(peer_id_);
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
    channel_->connect(router_info_.address, router_info_.port);
}

} // namespace base
