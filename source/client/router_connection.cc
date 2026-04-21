//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/router_connection.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"

namespace client {

//--------------------------------------------------------------------------------------------------
RouterConnection::RouterConnection(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RouterConnection::~RouterConnection()
{
    LOG(INFO) << "Dtor";
    onDisconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onConnectToRouter()
{
    if (status_ != Status::OFFLINE)
        return;

    LOG(INFO) << "Connecting to router " << config_.address << ":" << config_.port;

    setStatus(Status::CONNECTING);

    base::ClientAuthenticator* authenticator = new base::ClientAuthenticator(this);
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setSessionType(config_.session_type);
    authenticator->setUserName(config_.username);
    authenticator->setPassword(config_.password);

    tcp_channel_ = new base::TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &base::TcpChannel::sig_authenticated,
            this, &RouterConnection::onTcpReady);
    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred,
            this, &RouterConnection::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
            this, &RouterConnection::onTcpMessageReceived);

    tcp_channel_->connectTo(config_.address, config_.port);
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectFromRouter()
{
    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_->deleteLater();
        tcp_channel_ = nullptr;
    }

    setStatus(Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
const RouterConfig& RouterConnection::config() const
{
    return config_;
}

//--------------------------------------------------------------------------------------------------
const QUuid& RouterConnection::uuid() const
{
    return config_.uuid;
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onUpdateConfig(const RouterConfig& config)
{
    bool need_reconnect = !config_.hasSameConnectionParams(config);
    config_ = config;

    if (need_reconnect)
    {
        onDisconnectFromRouter();
        onConnectToRouter();
    }
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onRelayListRequest()
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
        return;

    proto::router::AdminToRouter message;
    proto::router::RelayListRequest* request = message.mutable_relay_list_request();
    request->set_dummy(1);

    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpReady()
{
    LOG(INFO) << "Connected to router " << config_.address << ":" << config_.port;
    setStatus(Status::ONLINE);
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Router connection error: " << error_code;

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_->deleteLater();
        tcp_channel_ = nullptr;
    }

    setStatus(Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_ADMIN)
    {
        if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
        {
            LOG(ERROR) << "Unexpected message from channel" << channel_id;
            return;
        }

        proto::router::RouterToAdmin message;
        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse admin message";
            return;
        }

        if (message.has_relay_list())
        {
            emit sig_relayListReceived(message.relay_list());
        }
        else
        {
            LOG(WARNING) << "Unhandled admin message";
            return;
        }
    }
    else if (channel_id == proto::router::CHANNEL_ID_MANAGER)
    {
        if (config_.session_type != proto::router::SESSION_TYPE_ADMIN &&
            config_.session_type != proto::router::SESSION_TYPE_MANAGER)
        {
            LOG(ERROR) << "Unexpected message from channel" << channel_id;
            return;
        }

        // TODO
    }
    else if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        proto::router::RouterToClient message;
        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse client message";
            return;
        }

        // TODO
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::setStatus(Status status)
{
    if (status_ != status)
    {
        status_ = status;
        emit sig_statusChanged(config_.uuid, status_);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::sendMessage(quint8 channel_id, const QByteArray& data)
{
    if (!tcp_channel_)
        return;

    tcp_channel_->send(channel_id, data);
}

} // namespace client
