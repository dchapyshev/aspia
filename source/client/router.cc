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
#include "base/version_constants.h"
#include "proto/router.pb.h"

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
        delete tcp_channel_;
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
void Router::connectToRouter(const QString& address, quint16 port)
{
    router_address_ = address;
    router_port_ = port;

    LOG(LS_INFO) << "Connecting to router " << router_address_ << ":" << router_port_;

    emit sig_connecting();

    tcp_channel_ = new base::TcpChannel(this);
    connect(tcp_channel_, &base::TcpChannel::sig_connected, this, &Router::onTcpConnected);
    tcp_channel_->connectTo(router_address_, router_port_);
}

//--------------------------------------------------------------------------------------------------
void Router::refreshSessionList()
{
    LOG(LS_INFO) << "Sending session list request";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;
    message.mutable_session_list_request()->set_dummy(1);
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::stopSession(qint64 session_id)
{
    LOG(LS_INFO) << "Sending disconnect request (session_id: " << session_id << ")";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;

    proto::router::SessionRequest* request = message.mutable_session_request();
    request->set_type(proto::router::SESSION_REQUEST_DISCONNECT);
    request->set_session_id(session_id);

    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::refreshUserList()
{
    LOG(LS_INFO) << "Sending user list request";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;
    message.mutable_user_list_request()->set_dummy(1);
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::addUser(const proto::router::User& user)
{
    LOG(LS_INFO) << "Sending user add request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;

    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_ADD);
    request->mutable_user()->CopyFrom(user);

    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::modifyUser(const proto::router::User& user)
{
    LOG(LS_INFO) << "Sending user modify request (username: " << user.name()
                 << ", entry_id: " << user.entry_id() << ")";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;

    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_MODIFY);
    request->mutable_user()->CopyFrom(user);

    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::deleteUser(qint64 entry_id)
{
    LOG(LS_INFO) << "Sending user delete request (entry_id: " << entry_id << ")";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;

    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_type(proto::router::USER_REQUEST_DELETE);
    request->mutable_user()->set_entry_id(entry_id);

    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectPeerSession(qint64 relay_session_id, quint64 peer_session_id)
{
    LOG(LS_INFO) << "Sending disconnect for peer session: " << peer_session_id
                 << " (relay: " << relay_session_id << ")";

    if (!tcp_channel_)
    {
        LOG(LS_ERROR) << "Tcp channel not connected yet";
        return;
    }

    proto::router::AdminToRouter message;

    proto::router::PeerConnectionRequest* request = message.mutable_peer_connection_request();
    request->set_relay_session_id(relay_session_id);
    request->set_peer_session_id(peer_session_id);
    request->set_type(proto::router::PEER_CONNECTION_REQUEST_DISCONNECT);

    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpConnected()
{
    LOG(LS_INFO) << "Router connected";

    reconnect_in_progress_ = false;
    reconnect_timer_->stop();
    timeout_timer_->stop();

    authenticator_ = new base::ClientAuthenticator(this);
    authenticator_->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator_->setSessionType(proto::router::SESSION_TYPE_ADMIN);
    authenticator_->setUserName(router_username_);
    authenticator_->setPassword(router_password_);

    connect(authenticator_, &base::Authenticator::sig_finished,
            this, [this](base::Authenticator::ErrorCode error_code)
    {
        if (error_code == base::Authenticator::ErrorCode::SUCCESS)
        {
            LOG(LS_INFO) << "Successful authentication";

            connect(tcp_channel_, &base::TcpChannel::sig_disconnected,
                    this, &Router::onTcpDisconnected);
            connect(tcp_channel_, &base::TcpChannel::sig_messageReceived,
                    this, &Router::onTcpMessageReceived);

            const QVersionNumber& router_version = authenticator_->peerVersion();
            const QVersionNumber& client_version = base::kCurrentVersion;

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
                tcp_channel_->setChannelIdSupport(true);
                tcp_channel_->resume();
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

    authenticator_->start(tcp_channel_);
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
        if (tcp_channel_)
        {
            tcp_channel_->deleteLater();
            tcp_channel_ = nullptr;
        }

        emit sig_waitForRouter();
        reconnect_timer_->start(std::chrono::seconds(5));
    }
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::RouterToAdmin message;

    if (!base::parse(buffer, &message))
    {
        LOG(LS_ERROR) << "Failed to read the message from the router";
        return;
    }

    if (message.has_session_list())
    {
        LOG(LS_INFO) << "Session list received";
        emit sig_sessionList(std::shared_ptr<proto::router::SessionList>(message.release_session_list()));
    }
    else if (message.has_session_result())
    {
        LOG(LS_INFO) << "Session result received with code: "
                     << message.session_result().error_code();
        emit sig_sessionResult(std::shared_ptr<proto::router::SessionResult>(message.release_session_result()));
    }
    else if (message.has_user_list())
    {
        LOG(LS_INFO) << "User list received";
        emit sig_userList(std::shared_ptr<proto::router::UserList>(message.release_user_list()));
    }
    else if (message.has_user_result())
    {
        LOG(LS_INFO) << "User result received with code: " << message.user_result().error_code();
        emit sig_userResult(std::shared_ptr<proto::router::UserResult>(message.release_user_result()));
    }
    else
    {
        LOG(LS_ERROR) << "Unknown message type from the router";
    }
}

} // namespace client
