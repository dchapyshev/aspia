//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/serialization.h"
#include "base/task_runner.h"
#include "base/peer/client_authenticator.h"
#include "host/host_key_storage.h"
#include "proto/router_peer.pb.h"

namespace host {

namespace {

const std::chrono::seconds kReconnectTimeout{ 10 };

} // namespace

//--------------------------------------------------------------------------------------------------
RouterController::RouterController(QObject* parent)
    : QObject(parent),
      peer_manager_(std::make_unique<base::RelayPeerManager>(this))
{
    LOG(LS_INFO) << "Ctor";

    reconnect_timer_.setSingleShot(true);
    connect(&reconnect_timer_, &QTimer::timeout, this, &RouterController::connectToRouter);
}

//--------------------------------------------------------------------------------------------------
RouterController::~RouterController()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void RouterController::hostIdRequest(const QString& session_name)
{
    LOG(LS_INFO) << "Started ID request for session '" << session_name << "'";

    if (!channel_ || !channel_->isConnected())
    {
        LOG(LS_INFO) << "No active connection to the router";
        return;
    }

    HostKeyStorage host_key_storage;
    QByteArray host_key = host_key_storage.key(session_name);

    pending_id_requests_.emplace(session_name);

    proto::PeerToRouter message;
    proto::HostIdRequest* host_id_request = message.mutable_host_id_request();

    if (host_key.isEmpty())
    {
        LOG(LS_INFO) << "Host key is empty. Request for a new ID";
        host_id_request->set_type(proto::HostIdRequest::NEW_ID);
    }
    else
    {
        LOG(LS_INFO) << "Host key is present. Request for an existing ID";
        host_id_request->set_type(proto::HostIdRequest::EXISTING_ID);
        host_id_request->set_key(host_key.toStdString());
    }

    // Send host ID request.
    LOG(LS_INFO) << "Send ID request to router";
    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterController::resetHostId(base::HostId host_id)
{
    LOG(LS_INFO) << "ResetHostId request: " << host_id;

    if (!channel_ || !channel_->isConnected())
    {
        LOG(LS_INFO) << "No active connection to the router";
        return;
    }

    proto::PeerToRouter message;
    message.mutable_reset_host_id()->set_host_id(host_id);
    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpConnected()
{
    DCHECK(channel_);

    LOG(LS_INFO) << "Connection to the router is established";

    channel_->setKeepAlive(true);
    channel_->setNoDelay(true);

    authenticator_ = std::make_unique<base::ClientAuthenticator>();

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(router_info_.public_key);
    authenticator_->setSessionType(proto::ROUTER_SESSION_HOST);

    connect(authenticator_.get(), &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            // The authenticator takes the listener on itself, we return the receipt of
            // notifications.
            channel_ = authenticator_->takeChannel();
            channel_->setListener(this);

            if (authenticator_->peerVersion() >= base::Version::kVersion_2_6_0)
            {
                LOG(LS_INFO) << "Using channel id support";
                channel_->setChannelIdSupport(true);
            }

            LOG(LS_INFO) << "Router connected";
            routerStateChanged(proto::internal::RouterState::CONNECTED);

            // Now the session will receive incoming messages.
            channel_->resume();
        }
        else
        {
            LOG(LS_ERROR) << "Authentication failed: "
                          << base::Authenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        authenticator_.release()->deleteLater();
    });

    authenticator_->start(std::move(channel_));
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::NetworkChannel::errorToString(error_code) << ")";

    routerStateChanged(proto::internal::RouterState::FAILED);
    delayedConnectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
{
    proto::RouterToPeer in_message;
    if (!base::parse(buffer, &in_message))
    {
        LOG(LS_ERROR) << "Invalid message from router";
        return;
    }

    if (in_message.has_host_id_response())
    {
        if (pending_id_requests_.empty())
        {
            LOG(LS_ERROR) << "ID received, but no request was sent";
            return;
        }

        const proto::HostIdResponse& host_id_response = in_message.host_id_response();

        switch (host_id_response.error_code())
        {
            case proto::HostIdResponse::SUCCESS:
                break;

            // The specified host is not in the router's database.
            case proto::HostIdResponse::NO_HOST_FOUND:
            {
                LOG(LS_INFO) << "Host is not in the router's database. Reset ID";

                proto::PeerToRouter out_message;
                out_message.mutable_host_id_request()->set_type(proto::HostIdRequest::NEW_ID);

                // Send host ID request.
                LOG(LS_INFO) << "Send ID request to router";
                channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(out_message));
                return;
            }

            default:
                LOG(LS_ERROR) << "Unknown error code: " << host_id_response.error_code();
                return;
        }

        base::HostId host_id = host_id_response.host_id();
        if (host_id == base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Invalid host ID received";
            return;
        }

        const QString& session_name = pending_id_requests_.front();
        HostKeyStorage host_key_storage;

        QByteArray host_key = QByteArray::fromStdString(host_id_response.key());
        if (!host_key.isEmpty())
        {
            LOG(LS_INFO) << "New host key received";
            host_key_storage.setKey(session_name, host_key);
        }

        LOG(LS_INFO) << "Host ID received: " << host_id << " session name: '" << session_name << "'";

        if (host_key_storage.lastHostId(session_name) != host_id)
            host_key_storage.setLastHostId(session_name, host_id);

        delegate_->onHostIdAssigned(session_name, host_id);
        pending_id_requests_.pop();
    }
    else if (in_message.has_connection_offer())
    {
        LOG(LS_INFO) << "New connection offer";

        const proto::ConnectionOffer& connection_offer = in_message.connection_offer();

        if (connection_offer.error_code() == proto::ConnectionOffer::SUCCESS &&
            connection_offer.peer_role() == proto::ConnectionOffer::HOST)
        {
            peer_manager_->addConnectionOffer(connection_offer);
        }
        else
        {
            LOG(LS_ERROR) << "Invalid connection offer";
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void RouterController::onNewPeerConnected(std::unique_ptr<base::TcpChannel> channel)
{
    LOG(LS_INFO) << "New peer connected";

    if (delegate_)
    {
        delegate_->onClientConnected(std::move(channel));
    }
    else
    {
        LOG(LS_ERROR) << "Invalid delegate";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    routerStateChanged(proto::internal::RouterState::CONNECTING);

    channel_ = std::make_unique<base::TcpChannel>();
    channel_->setListener(this);
    channel_->connect(router_info_.address, router_info_.port);
}

//--------------------------------------------------------------------------------------------------
void RouterController::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_.start(kReconnectTimeout);
}

//--------------------------------------------------------------------------------------------------
void RouterController::routerStateChanged(proto::internal::RouterState::State state)
{
    LOG(LS_INFO) << "Router state changed: " << routerStateToString(state);

    if (!delegate_)
    {
        LOG(LS_INFO) << "Invalid delegate";
        return;
    }

    proto::internal::RouterState router_state;
    router_state.set_state(state);

    router_state.set_host_name(router_info_.address.toStdString());
    router_state.set_host_port(router_info_.port);

    delegate_->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
// static
const char* RouterController::routerStateToString(proto::internal::RouterState::State state)
{
    switch (state)
    {
        case proto::internal::RouterState::DISABLED:
            return "DISABLED";

        case proto::internal::RouterState::CONNECTING:
            return "CONNECTING";

        case proto::internal::RouterState::CONNECTED:
            return "CONNECTED";

        case proto::internal::RouterState::FAILED:
            return "FAILED";

        default:
            return "UNKNOWN";
    }
}

} // namespace host
