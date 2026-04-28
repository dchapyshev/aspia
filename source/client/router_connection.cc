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

#include <QHash>
#include <QTimer>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "build/build_config.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"

namespace client {

namespace {

const std::chrono::seconds kReconnectTimeout{ 5 };

//--------------------------------------------------------------------------------------------------
QHash<qint64, RouterConnection*>& instances()
{
    static thread_local QHash<qint64, RouterConnection*> g_instances;
    return g_instances;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterConnection::RouterConnection(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnecting to router" << config_.address;
        onConnectToRouter();
    });
}

//--------------------------------------------------------------------------------------------------
RouterConnection::~RouterConnection()
{
    LOG(INFO) << "Dtor";
    instances().remove(config_.router_id);
    onDisconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
// static
RouterConnection* RouterConnection::instance(qint64 router_id)
{
    if (!instances().contains(router_id))
        return nullptr;

    return instances().value(router_id);
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onConnectToRouter()
{
    // We cannot perform registration in the constructor because the constructor is executed in the
    // GUI thread.
    instances().insert(config_.router_id, this);

    reconnect_timer_->stop();

    if (status_ != Status::OFFLINE)
        return;

    LOG(INFO) << "Connecting to router" << config_.address;

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

    base::Address address = base::Address::fromString(config_.address, DEFAULT_ROUTER_TCP_PORT);
    tcp_channel_->connectTo(address.host(), address.port());
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectFromRouter()
{
    reconnect_timer_->stop();

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
qint64 RouterConnection::routerId() const
{
    return config_.router_id;
}

//--------------------------------------------------------------------------------------------------
QVersionNumber RouterConnection::version() const
{
    if (!tcp_channel_)
        return QVersionNumber();

    return tcp_channel_->peerVersion();
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
    request->set_command_name("add");
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
    request->set_command_name("modify");
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
    request->set_command_name("delete");
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
    request->set_command_name("disconnect");
    request->set_entry_id(session_id);

    LOG(INFO) << "Sending host disconnect request (entry_id:" << session_id << ")";
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
    request->set_command_name("remove");
    request->set_entry_id(session_id);
    if (try_to_uninstall)
        request->set_params("try_to_uninstall");

    LOG(INFO) << "Sending host remove request (entry_id:" << session_id
              << "try_to_uninstall:" << try_to_uninstall << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectRelay(qint64 session_id)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::RelayRequest* request = message.mutable_relay_request();
    request->set_command_name("disconnect");
    request->set_entry_id(session_id);

    LOG(INFO) << "Sending relay disconnect request (entry_id:" << session_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onDisconnectPeer(qint64 relay_entry_id, quint64 peer_session_id)
{
    if (config_.session_type != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::PeerRequest* request = message.mutable_peer_request();
    request->set_relay_session_id(relay_entry_id);
    request->set_peer_session_id(peer_session_id);
    request->set_command_name("disconnect");

    LOG(INFO) << "Sending peer disconnect request (relay_entry_id:" << relay_entry_id
              << "peer_session_id:" << peer_session_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onConnectionRequest(qint64 request_id, quint64 host_id)
{
    proto::router::ClientToRouter message;
    proto::router::ConnectionRequest* request = message.mutable_connection_request();
    request->set_request_id(request_id);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending connection request:" << *request;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onCheckHostStatus(qint64 request_id, quint64 host_id)
{
    proto::router::ClientToRouter message;
    proto::router::CheckHostStatus* request = message.mutable_check_host_status();
    request->set_request_id(request_id);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending check host status:" << *request;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpReady()
{
    CHECK(tcp_channel_);

    LOG(INFO) << "Connected to router" << config_.address;
    reconnect_timer_->stop();
    setStatus(Status::ONLINE);

    tcp_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void RouterConnection::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Router connection error:" << error_code;

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_->deleteLater();
        tcp_channel_ = nullptr;
    }

    emit sig_errorOccurred(config_.router_id, error_code);

    setStatus(Status::OFFLINE);

    LOG(INFO) << "Reconnect scheduled in" << kReconnectTimeout.count() << "seconds";
    reconnect_timer_->start(kReconnectTimeout);
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
        else if (message.has_relay_result())
        {
            LOG(INFO) << "Relay result received";
            emit sig_relayResultReceived(message.relay_result());
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

        if (message.has_connection_offer())
        {
            emit sig_connectionOffer(message.connection_offer());
        }
        else if (message.has_host_status())
        {
            const proto::router::HostStatus& host_status = message.host_status();
            bool online = host_status.status() == proto::router::HostStatus::STATUS_ONLINE;
            emit sig_hostStatus(host_status.request_id(), online);
        }
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
        emit sig_statusChanged(config_.router_id, status_);
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
