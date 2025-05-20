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

#include "client/router_controller.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_peer.pb.h"

namespace client {

namespace {

auto g_errorType = qRegisterMetaType<client::RouterController::Error>();

} // namespace

//--------------------------------------------------------------------------------------------------
RouterController::RouterController(const RouterConfig& router_config, QObject* parent)
    : QObject(parent),
      status_request_timer_(new QTimer(this)),
      router_config_(router_config)
{
    LOG(LS_INFO) << "Ctor";

    status_request_timer_->setSingleShot(true);

    connect(status_request_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Request host status from router (host_id=" << host_id_ << ")";

        proto::PeerToRouter message;
        message.mutable_check_host_status()->set_host_id(host_id_);
        router_channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
    });
}

//--------------------------------------------------------------------------------------------------
RouterController::~RouterController()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterController::connectTo(base::HostId host_id, bool wait_for_host)
{
    host_id_ = host_id;
    wait_for_host_ = wait_for_host;

    DCHECK_NE(host_id_, base::kInvalidHostId);

    LOG(LS_INFO) << "Connecting to router... (host_id=" << host_id_ << " wait_for_host="
                 << wait_for_host_ << ")";

    router_channel_ = new base::TcpChannel(this);

    connect(router_channel_, &base::TcpChannel::sig_connected,
            this, &RouterController::onTcpConnected);

    router_channel_->connect(router_config_.address, router_config_.port);
}

//--------------------------------------------------------------------------------------------------
base::TcpChannel* RouterController::takeChannel()
{
    return host_channel_.release();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpConnected()
{
    LOG(LS_INFO) << "Connection to the router is established";

    router_channel_->setKeepAlive(true);
    router_channel_->setNoDelay(true);

    authenticator_ = new base::ClientAuthenticator(this);

    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setUserName(router_config_.username);
    authenticator_->setPassword(router_config_.password);
    authenticator_->setSessionType(proto::ROUTER_SESSION_CLIENT);

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            const QVersionNumber& router_version = authenticator_->peerVersion();

            emit sig_routerConnected(router_version);

            connect(router_channel_, &base::TcpChannel::sig_disconnected,
                    this, &RouterController::onTcpDisconnected);
            connect(router_channel_, &base::TcpChannel::sig_messageReceived,
                    this, &RouterController::onTcpMessageReceived);

            if (router_version >= base::kVersion_2_6_0)
            {
                LOG(LS_INFO) << "Using channel id support";
                router_channel_->setChannelIdSupport(true);
            }

            LOG(LS_INFO) << "Sending connection request (host_id: " << host_id_ << ")";

            // Now the session will receive incoming messages.
            router_channel_->resume();

            sendConnectionRequest();
        }
        else
        {
            LOG(LS_ERROR) << "Authentication failed: "
                          << base::Authenticator::errorToString(error_code);

            Error error;
            error.type = ErrorType::AUTHENTICATION;
            error.code.authentication = error_code;

            emit sig_errorOccurred(error);
        }

        // Authenticator is no longer needed.
        authenticator_->deleteLater();
        authenticator_ = nullptr;
    });

    authenticator_->start(router_channel_);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Connection to the router is lost ("
                 << base::TcpChannel::errorToString(error_code) << ")";

    Error error;
    error.type = ErrorType::NETWORK;
    error.code.network = error_code;

    emit sig_errorOccurred(error);
}

//--------------------------------------------------------------------------------------------------
void RouterController::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    Error error;
    error.type = ErrorType::ROUTER;

    proto::RouterToPeer message;
    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from router";

        error.code.router = ErrorCode::UNKNOWN_ERROR;
        emit sig_errorOccurred(error);
        return;
    }

    if (message.has_connection_offer())
    {
        if (relay_peer_)
        {
            LOG(LS_ERROR) << "Re-offer connection detected";
            return;
        }

        const proto::ConnectionOffer& connection_offer = message.connection_offer();

        if (connection_offer.error_code() != proto::ConnectionOffer::SUCCESS ||
            connection_offer.peer_role() != proto::ConnectionOffer::CLIENT)
        {
            switch (connection_offer.error_code())
            {
                case proto::ConnectionOffer::PEER_NOT_FOUND:
                    error.code.router = ErrorCode::PEER_NOT_FOUND;
                    break;

                case proto::ConnectionOffer::ACCESS_DENIED:
                    error.code.router = ErrorCode::ACCESS_DENIED;
                    break;

                case proto::ConnectionOffer::KEY_POOL_EMPTY:
                    error.code.router = ErrorCode::KEY_POOL_EMPTY;
                    break;

                default:
                    error.code.router = ErrorCode::UNKNOWN_ERROR;
                    break;
            }

            if (error.code.router == ErrorCode::PEER_NOT_FOUND && wait_for_host_)
            {
                LOG(LS_INFO) << "Host is OFFLINE. Wait for host";
                waitForHost();
            }
            else
            {
                emit sig_errorOccurred(error);
            }
        }
        else
        {
            relay_peer_ = new base::RelayPeer(this);

            connect(relay_peer_, &base::RelayPeer::sig_connectionError,
                    this, &RouterController::onRelayConnectionError);
            connect(relay_peer_, &base::RelayPeer::sig_connectionReady,
                    this, &RouterController::onRelayConnectionReady);

            relay_peer_->start(connection_offer);
        }
    }
    else if (message.has_host_status())
    {
        const proto::HostStatus& host_status = message.host_status();
        if (host_status.status() == proto::HostStatus::STATUS_ONLINE)
        {
            LOG(LS_INFO) << "Host is ONLINE now. Sending connection request";
            sendConnectionRequest();
        }
        else
        {
            LOG(LS_INFO) << "Host is OFFLINE. Wait for host";
            waitForHost();
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRelayConnectionReady()
{
    LOG(LS_INFO) << "Relay connection ready";

    if (!relay_peer_)
    {
        LOG(LS_ERROR) << "No relay peer instance";
        return;
    }

    host_channel_.reset(relay_peer_->takeChannel());

    emit sig_hostConnected();
    relay_peer_->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void RouterController::onRelayConnectionError()
{
    LOG(LS_INFO) << "Relay connection error";

    Error error;
    error.type = ErrorType::ROUTER;
    error.code.router = ErrorCode::RELAY_ERROR;

    emit sig_errorOccurred(error);
    relay_peer_->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void RouterController::sendConnectionRequest()
{
    LOG(LS_INFO) << "Sending connection request (host_id=" << host_id_ << ")";

    proto::PeerToRouter message;
    message.mutable_connection_request()->set_host_id(host_id_);
    router_channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterController::waitForHost()
{
    LOG(LS_INFO) << "Wait for host";
    emit sig_hostAwaiting();
    status_request_timer_->start(std::chrono::milliseconds(5000));
}

} // namespace client
