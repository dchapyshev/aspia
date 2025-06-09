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
#include "host/host_storage.h"
#include "proto/router_peer.pb.h"

namespace host {

namespace {

const std::chrono::seconds kReconnectTimeout{ 10 };

} // namespace

//--------------------------------------------------------------------------------------------------
RouterController::RouterController(QObject* parent)
    : QObject(parent),
      peer_manager_(new base::RelayPeerManager(this)),
      reconnect_timer_(new QTimer(this))
{
    LOG(LS_INFO) << "Ctor";

    connect(peer_manager_, &base::RelayPeerManager::sig_newPeerConnected,
            this, &RouterController::onNewPeerConnected);

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, &RouterController::connectToRouter);
}

//--------------------------------------------------------------------------------------------------
RouterController::~RouterController()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterController::start(const QString& address, quint16 port, const QByteArray& public_key)
{
    address_ = address;
    port_ = port;
    public_key_ = public_key;

    LOG(LS_INFO) << "Starting host controller for router: " << address_ << ":" << port_;
    connectToRouter();
}

//--------------------------------------------------------------------------------------------------
bool RouterController::hasPendingConnections() const
{
    return !channels_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
base::TcpChannel* RouterController::nextPendingConnection()
{
    if (channels_.isEmpty())
        return nullptr;

    base::TcpChannel* channel = channels_.front();
    channel->setParent(nullptr);

    channels_.pop_front();
    return channel;
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpConnected()
{
    DCHECK(tcp_channel_);

    LOG(LS_INFO) << "Connection to the router is established";

    authenticator_ = new base::ClientAuthenticator(this);

    authenticator_->setIdentify(proto::IDENTIFY_ANONYMOUS);
    authenticator_->setPeerPublicKey(public_key_);
    authenticator_->setSessionType(proto::router::SESSION_TYPE_HOST);

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            connect(tcp_channel_, &base::TcpChannel::sig_disconnected,
                    this, &RouterController::onTcpDisconnected);
            connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
                    this, &RouterController::onTcpMessageReceived);

            LOG(LS_INFO) << "Router connected";
            routerStateChanged(proto::internal::RouterState::CONNECTED);

            // Now the session will receive incoming messages.
            tcp_channel_->setChannelIdSupport(true);
            tcp_channel_->resume();

            hostIdRequest();
        }
        else
        {
            LOG(LS_ERROR) << "Authentication failed: "
                          << base::Authenticator::errorToString(error_code);
            delayedConnectToRouter();
        }

        // Authenticator is no longer needed.
        authenticator_->deleteLater();
        authenticator_ = nullptr;
    });

    authenticator_->start(tcp_channel_);
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
void RouterController::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::RouterToPeer in_message;
    if (!base::parse(buffer, &in_message))
    {
        LOG(LS_ERROR) << "Invalid message from router";
        return;
    }

    if (in_message.has_host_id_response())
    {
        const proto::router::HostIdResponse& host_id_response = in_message.host_id_response();

        switch (host_id_response.error_code())
        {
            case proto::router::HostIdResponse::SUCCESS:
                break;

            // The specified host is not in the router's database.
            case proto::router::HostIdResponse::NO_HOST_FOUND:
            {
                LOG(LS_INFO) << "Host is not in the router's database. Reset ID";

                proto::router::PeerToRouter out_message;
                out_message.mutable_host_id_request()->set_type(proto::router::HostIdRequest::NEW_ID);

                // Send host ID request.
                LOG(LS_INFO) << "Send ID request to router";
                tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(out_message));
                return;
            }

            default:
                LOG(LS_ERROR) << "Unknown error code: " << host_id_response.error_code();
                return;
        }

        host_id_ = host_id_response.host_id();
        if (host_id_ == base::kInvalidHostId)
        {
            LOG(LS_ERROR) << "Invalid host ID received";
            return;
        }

        HostStorage host_key_storage;

        QByteArray host_key = QByteArray::fromStdString(host_id_response.key());
        if (!host_key.isEmpty())
        {
            LOG(LS_INFO) << "New host key received";
            host_key_storage.setHostKey(host_key);
        }

        LOG(LS_INFO) << "Host ID received: " << host_id_;

        if (host_key_storage.lastHostId() != host_id_)
            host_key_storage.setLastHostId(host_id_);

        emit sig_hostIdAssigned(host_id_);
    }
    else if (in_message.has_connection_offer())
    {
        LOG(LS_INFO) << "New connection offer";

        const proto::router::ConnectionOffer& connection_offer = in_message.connection_offer();

        if (connection_offer.error_code() == proto::router::ConnectionOffer::SUCCESS &&
            connection_offer.peer_role() == proto::router::ConnectionOffer::HOST)
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
void RouterController::onNewPeerConnected()
{
    LOG(LS_INFO) << "New peer connected";
    channels_ = peer_manager_->takePendingConnections();

    for (const auto& channel : std::as_const(channels_))
        channel->setParent(this);

    emit sig_clientConnected();
}

//--------------------------------------------------------------------------------------------------
void RouterController::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    routerStateChanged(proto::internal::RouterState::CONNECTING);

    tcp_channel_ = new base::TcpChannel(this);
    connect(tcp_channel_, &base::TcpChannel::sig_connected, this, &RouterController::onTcpConnected);

    tcp_channel_->connectTo(address_, port_);
}

//--------------------------------------------------------------------------------------------------
void RouterController::delayedConnectToRouter()
{
    LOG(LS_INFO) << "Reconnect after " << kReconnectTimeout.count() << " seconds";
    reconnect_timer_->start(kReconnectTimeout);
}

//--------------------------------------------------------------------------------------------------
void RouterController::routerStateChanged(proto::internal::RouterState::State state)
{
    LOG(LS_INFO) << "Router state changed: " << state;

    router_state_.set_state(state);
    router_state_.set_host_name(address_.toStdString());
    router_state_.set_host_port(port_);

    emit sig_routerStateChanged(router_state_);
}

//--------------------------------------------------------------------------------------------------
void RouterController::hostIdRequest()
{
    LOG(LS_INFO) << "Started ID request for session";

    if (!tcp_channel_ || !tcp_channel_->isConnected())
    {
        LOG(LS_INFO) << "No active connection to the router";
        return;
    }

    HostStorage host_key_storage;
    QByteArray host_key = host_key_storage.hostKey();

    proto::router::PeerToRouter message;
    proto::router::HostIdRequest* host_id_request = message.mutable_host_id_request();

    if (host_key.isEmpty())
    {
        LOG(LS_INFO) << "Host key is empty. Request for a new ID";
        host_id_request->set_type(proto::router::HostIdRequest::NEW_ID);
    }
    else
    {
        LOG(LS_INFO) << "Host key is present. Request for an existing ID";
        host_id_request->set_type(proto::router::HostIdRequest::EXISTING_ID);
        host_id_request->set_key(host_key.toStdString());
    }

    // Send host ID request.
    LOG(LS_INFO) << "Send ID request to router";
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

} // namespace host
