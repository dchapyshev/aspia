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

#include "client/router.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/router_common.pb.h"

namespace client {

//--------------------------------------------------------------------------------------------------
Router::Router(QObject* parent)
    : QObject(parent),
      timeout_timer_(new QTimer(this)),
      reconnect_timer_(new QTimer(this))
{
    LOG(LS_INFO) << "Ctor";

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Reconnecting to router";
        connectToRouter(router_address_, router_port_);
    });

    timeout_timer_->setSingleShot(true);
    connect(timeout_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Reconnect timeout";
        emit sig_waitForRouterTimeout();

        reconnect_timer_->stop();
        reconnect_in_progress_ = false;
        setAutoReconnect(false);
        delete channel_;
    });
}

//--------------------------------------------------------------------------------------------------
Router::~Router()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Router::setUserName(const QString& username)
{
    router_username_ = username;
}

//--------------------------------------------------------------------------------------------------
void Router::setPassword(const QString& password)
{
    router_password_ = password;
}

//--------------------------------------------------------------------------------------------------
void Router::setAutoReconnect(bool enable)
{
    auto_reconnect_ = enable;
}

//--------------------------------------------------------------------------------------------------
bool Router::isAutoReconnect() const
{
    return auto_reconnect_;
}

//--------------------------------------------------------------------------------------------------
void Router::connectToRouter(const QString& address, uint16_t port)
{
    router_address_ = address;
    router_port_ = port;

    LOG(LS_INFO) << "Connecting to router " << router_address_ << ":" << router_port_;

    emit sig_connecting();

    channel_ = new base::TcpChannel(this);
    connect(channel_, &base::TcpChannel::sig_connected, this, &Router::onTcpConnected);
    channel_->connect(router_address_, router_port_);
}

//--------------------------------------------------------------------------------------------------
void Router::refreshSessionList()
{
    LOG(LS_INFO) << "Sending session list request";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;
    message.mutable_session_list_request()->set_dummy(1);
    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::stopSession(int64_t session_id)
{
    LOG(LS_INFO) << "Sending disconnect request (session_id: " << session_id << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;

    proto::SessionRequest* request = message.mutable_session_request();
    request->set_type(proto::SESSION_REQUEST_DISCONNECT);
    request->set_session_id(session_id);

    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::refreshUserList()
{
    LOG(LS_INFO) << "Sending user list request";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;
    message.mutable_user_list_request()->set_dummy(1);
    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::addUser(const proto::User& user)
{
    LOG(LS_INFO) << "Sending user add request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_ADD);
    request->mutable_user()->CopyFrom(user);

    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::modifyUser(const proto::User& user)
{
    LOG(LS_INFO) << "Sending user modify request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_MODIFY);
    request->mutable_user()->CopyFrom(user);

    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::deleteUser(int64_t entry_id)
{
    LOG(LS_INFO) << "Sending user delete request (entry_id: " << entry_id << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;

    proto::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::USER_REQUEST_DELETE);
    request->mutable_user()->set_entry_id(entry_id);

    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectPeerSession(int64_t relay_session_id, uint64_t peer_session_id)
{
    LOG(LS_INFO) << "Sending disconnect for peer session: " << peer_session_id
                 << " (relay: " << relay_session_id << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::AdminToRouter message;

    proto::PeerConnectionRequest* request = message.mutable_peer_connection_request();
    request->set_relay_session_id(relay_session_id);
    request->set_peer_session_id(peer_session_id);
    request->set_type(proto::PEER_CONNECTION_REQUEST_DISCONNECT);

    channel_->send(proto::ROUTER_CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpConnected()
{
    LOG(LS_INFO) << "Router connected";

    reconnect_in_progress_ = false;
    reconnect_timer_->stop();
    timeout_timer_->stop();

    channel_->setKeepAlive(true);
    channel_->setNoDelay(true);

    authenticator_ = new base::ClientAuthenticator(this);
    authenticator_->setIdentify(proto::IDENTIFY_SRP);
    authenticator_->setSessionType(proto::ROUTER_SESSION_ADMIN);
    authenticator_->setUserName(router_username_);
    authenticator_->setPassword(router_password_);

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            LOG(LS_INFO) << "Successful authentication";

            connect(channel_, &base::TcpChannel::sig_disconnected,
                    this, &Router::onTcpDisconnected);
            connect(channel_, &base::TcpChannel::sig_messageReceived,
                    this, &Router::onTcpMessageReceived);

            const base::Version& router_version = authenticator_->peerVersion();
            if (router_version >= base::Version::kVersion_2_6_0)
            {
                LOG(LS_INFO) << "Using channel id support";
                channel_->setChannelIdSupport(true);
            }

            const base::Version& client_version = base::Version::kCurrentFullVersion;
            if (router_version > client_version)
            {
                LOG(LS_ERROR) << "Version mismatch (router: " << router_version.toString()
                << " client: " << client_version.toString();
                emit sig_versionMismatch(router_version, client_version);
            }
            else
            {
                emit sig_connected(router_version);

                // Now the session will receive incoming messages.
                channel_->resume();
            }
        }
        else
        {
            LOG(LS_INFO) << "Failed authentication: "
                         << base::Authenticator::errorToString(error_code);
            emit sig_accessDenied(error_code);
        }

        // Authenticator is no longer needed.
        authenticator_->deleteLater();
        authenticator_ = nullptr;
    });

    authenticator_->start(channel_);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Router disconnected: " << base::NetworkChannel::errorToString(error_code);
    emit sig_disconnected(error_code);

    if (isAutoReconnect())
    {
        LOG(LS_INFO) << "Reconnect to router enabled";

        timeout_timer_->start(std::chrono::minutes(5));
        reconnect_in_progress_ = true;

        // Delete old channel.
        if (channel_)
        {
            channel_->deleteLater();
            channel_ = nullptr;
        }

        emit sig_waitForRouter();
        reconnect_timer_->start(std::chrono::seconds(5));
    }
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
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
        emit sig_sessionList(std::shared_ptr<proto::SessionList>(message.release_session_list()));
    }
    else if (message.has_session_result())
    {
        LOG(LS_INFO) << "Session result received with code: "
                     << message.session_result().error_code();
        emit sig_sessionResult(std::shared_ptr<proto::SessionResult>(message.release_session_result()));
    }
    else if (message.has_user_list())
    {
        LOG(LS_INFO) << "User list received";
        emit sig_userList(std::shared_ptr<proto::UserList>(message.release_user_list()));
    }
    else if (message.has_user_result())
    {
        LOG(LS_INFO) << "User result received with code: " << message.user_result().error_code();
        emit sig_userResult(std::shared_ptr<proto::UserResult>(message.release_user_result()));
    }
    else
    {
        LOG(LS_ERROR) << "Unknown message type from the router";
    }
}

} // namespace client
