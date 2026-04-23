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
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::RelayListRequest* request = message.mutable_relay_list_request();
    request->set_dummy(1);

    LOG(INFO) << "Sending relay list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onHostListRequest()
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostListRequest* request = message.mutable_host_list_request();
    request->set_dummy(1);

    LOG(INFO) << "Sending host list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onUserListRequest()
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    message.mutable_user_list_request()->set_dummy(1);

    LOG(INFO) << "Sending user list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onAddUser(const proto::router::User& user)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_ADD);
    request->mutable_user()->CopyFrom(user);

    LOG(INFO) << "Sending user add request (username:" << user.name()
              << ", entry_id:" << user.entry_id() << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onModifyUser(const proto::router::User& user)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_MODIFY);
    request->mutable_user()->CopyFrom(user);

    LOG(INFO) << "Sending user modify request (username:" << user.name()
              << ", entry_id:" << user.entry_id() << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDeleteUser(qint64 entry_id)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_DELETE);
    request->mutable_user()->set_entry_id(entry_id);

    LOG(INFO) << "Sending user delete request (entry_id:" << entry_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectHost(qint64 session_id)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostRequest* request = message.mutable_host_request();
    request->set_type("disconnect");
    request->set_session_id(session_id);

    LOG(INFO) << "Sending host disconnect request (session_id:" << session_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectAllHosts()
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostRequest* request = message.mutable_host_request();
    request->set_type("disconnect_all");

    LOG(INFO) << "Sending host disconnect all request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onRemoveHost(qint64 session_id, bool try_to_uninstall)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostRequest* request = message.mutable_host_request();
    request->set_type("remove");
    request->set_session_id(session_id);
    if (try_to_uninstall)
        request->set_params("try_to_uninstall");

    LOG(INFO) << "Sending host remove request (session_id:" << session_id
              << "try_to_uninstall:" << try_to_uninstall << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpReady()
{
    CHECK(tcp_channel_);

    LOG(INFO) << "Connected to router " << config_.address << ":" << config_.port;
    setStatus(Status::ONLINE);

    tcp_channel_->setPaused(false);
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
            LOG(INFO) << "Relay list received";
            emit sig_relayListReceived(message.relay_list());
        }
        else if (message.has_host_list())
        {
            LOG(INFO) << "Host list received";
            emit sig_hostListReceived(message.host_list());
        }
        else if (message.has_user_list())
        {
            LOG(INFO) << "User list received";
            emit sig_userListReceived(message.user_list());
        }
        else if (message.has_user_result())
        {
            LOG(INFO) << "User result received";
            emit sig_userResultReceived(message.user_result());
        }
        else if (message.has_host_result())
        {
            LOG(INFO) << "Host result received";
            emit sig_hostResultReceived(message.host_result());
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
        LOG(INFO) << "Status changed from" << status_ << "to" << status;
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
