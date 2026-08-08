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

#include "host/router_manager.h"

#include "base/bitset.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/sys_info.h"
#include "base/crypto/password_generator.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "base/peer/relay_peer_manager.h"
#include "base/peer/server_authenticator.h"
#include "base/threading/worker.h"
#include "host/database.h"
#include "host/host_storage.h"
#include "proto/key_exchange.h"
#include "proto/router_constants.h"
#include "proto/router_host.h"

namespace {

const Seconds kReconnectTimeout{ 10 };

// How long the private half of an issued connection key waits for its offer.
const Seconds kConnectionKeyTtl{ 60 };

// Outstanding connection keys, requests over that are refused.
const size_t kMaxPendingConnectionKeys = 16;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterManager::RouterManager(Database& database, QObject* parent)
    : QObject(parent),
      database_(database),
      peer_manager_(new RelayPeerManager(this)),
      user_list_(new HostUserList(database))
{
    LOG(INFO) << "Ctor";

    connect(peer_manager_, &RelayPeerManager::sig_newPeerConnected,
            this, &RouterManager::onNewPeerConnected);
    connect(Worker::current(), &Worker::sig_tick, this, &RouterManager::onTimer);
}

//--------------------------------------------------------------------------------------------------
RouterManager::~RouterManager()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool RouterManager::hasReadyConnections() const
{
    return !channels_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
std::optional<RouterManager::ReadyConnection> RouterManager::nextReadyConnection()
{
    if (channels_.isEmpty())
        return std::nullopt;

    ReadyConnection ready = channels_.takeFirst();
    ready.tcp_channel->setParent(nullptr);
    return ready;
}

//--------------------------------------------------------------------------------------------------
void RouterManager::start()
{
    onSettingsChanged();
    connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onSettingsChanged()
{
    Address new_address = database_.routerAddress();
    QByteArray new_public_key = database_.routerPublicKey();

    // Check if the connection parameters have changed.
    if (router_address_ != new_address || public_key_ != new_public_key)
    {
        router_address_ = new_address;
        public_key_ = new_public_key;

        if (tcp_channel_)
        {
            tcp_channel_->disconnect(this);
            tcp_channel_.reset();
            pending_connection_keys_.clear();
        }

        connectToRouter();
    }

    if (!database_.oneTimePassword())
    {
        LOG(INFO) << "One-time password is disabled";
        password_expire_time_ = TimePoint::max();
        one_time_sessions_ = 0;
        one_time_password_.clear();
        user_list_->setOneTimeUser(User());
    }
    else
    {
        LOG(INFO) << "One-time password is enabled";

        if (one_time_password_.isEmpty())
            renewOneTimePassword();
        else
            user_list_->setOneTimeUser(createOneTimeUser());
    }

    emit sig_credentialsChanged(host_id_, one_time_password_);
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onOneTimeSessionsChanged(quint32 one_time_sessions)
{
    one_time_sessions_ = one_time_sessions;
    onSettingsChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onNewOneTimePassword()
{
    if (!database_.oneTimePassword())
    {
        LOG(INFO) << "One-time password is disabled";
        return;
    }

    renewOneTimePassword();
    emit sig_credentialsChanged(host_id_, one_time_password_);
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onUserSessionAttached()
{
    emit sig_routerStateChanged(router_state_);
    emit sig_credentialsChanged(host_id_, one_time_password_);
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onTcpReady()
{
    DCHECK(tcp_channel_);

    LOG(INFO) << "Connection to the router is established";
    routerStateChanged(proto::user::RouterState::CONNECTED);

    // Now the session will receive incoming messages.
    reconnect_time_ = TimePoint::max();
    tcp_channel_->setPaused(false);
    hostIdRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Connection to the router is lost:" << error_code;

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_.reset();
    }

    // The offers referencing the issued connection keys can only arrive over the lost channel.
    pending_connection_keys_.clear();

    // The host is not reachable by the assigned ID until the connection is restored and the router
    // re-assigns it, so report the credentials as unavailable.
    host_id_ = kInvalidHostId;
    emit sig_credentialsChanged(host_id_, one_time_password_);

    routerStateChanged(proto::user::RouterState::FAILED);
    delayedConnectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    proto::router::RouterToHost in_message;
    if (!parse(buffer, &in_message))
    {
        LOG(ERROR) << "Invalid message from router";
        return;
    }

    if (in_message.has_host_id_response())
    {
        const proto::router::HostIdResponse& host_id_response = in_message.host_id_response();
        const std::string& error_code = host_id_response.error_code();

        if (error_code == proto::router::kErrorOk)
        {
            // Continue below.
        }
        else if (error_code == proto::router::kErrorNotFound)
        {
            // The specified host is not in the router's database.
            LOG(INFO) << "Host is not in the router's database. Reset ID";

            proto::router::HostToRouter out_message;
            proto::router::HostIdRequest* host_id_request = out_message.mutable_host_id_request();
            host_id_request->set_type(proto::router::HostIdRequest::NEW_ID);
            host_id_request->set_hw_id(SysInfo::hardwareId().toStdString());

            // Send host ID request.
            LOG(INFO) << "Send ID request to router";
            tcp_channel_->send(0, serialize(out_message));
            return;
        }
        else
        {
            LOG(ERROR) << "Error code from router:" << QString::fromStdString(error_code);
            return;
        }

        host_id_ = host_id_response.host_id();
        if (host_id_ == kInvalidHostId)
        {
            LOG(ERROR) << "Invalid host ID received";
            return;
        }

        HostStorage host_storage;

        QByteArray host_key = QByteArray::fromStdString(host_id_response.key());
        if (!host_key.isEmpty())
        {
            LOG(INFO) << "New host key received";
            host_storage.setHostKey(host_key);
        }

        LOG(INFO) << "Host ID received:" << host_id_;

        if (host_storage.lastHostId() != host_id_)
            host_storage.setLastHostId(host_id_);

        // The one-time user name is derived from the host ID ("#<id>"), so the user can only be
        // registered now that the ID is known; before this point (first connect or reconnect after
        // a connection loss) it was created invalid.
        user_list_->setOneTimeUser(createOneTimeUser());

        emit sig_credentialsChanged(host_id_, one_time_password_);
    }
    else if (in_message.has_connection_offer())
    {
        readConnectionOffer(in_message.connection_offer());
    }
    else if (in_message.has_connection_key_request())
    {
        readConnectionKeyRequest(in_message.connection_key_request());
    }
    else if (in_message.has_host_command())
    {
        const proto::router::HostCommand& command = in_message.host_command();
        const std::string& command_name = command.command_name();

        if (command_name == "remove")
        {
            LOG(INFO) << "Remove command received";
            emit sig_removeHost();
        }
        else if (command_name == "update")
        {
            LOG(INFO) << "Update check command received";
            emit sig_checkUpdates();
        }
        else
        {
            LOG(ERROR) << "Unknown host command:" << QString::fromStdString(command_name);
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from router";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onNewPeerConnected()
{
    LOG(INFO) << "New peer connected";
    CHECK(peer_manager_);

    while (peer_manager_->hasPendingConnections())
    {
        auto ready = peer_manager_->takePendingConnection();

        const proto::router::ConnectionOffer& offer = ready.second;
        TcpChannel* tcp_channel = ready.first;

        tcp_channel->setParent(this);

        ReadyConnection connection;
        connection.tcp_channel = ready.first;

        if (offer.has_stun_info())
        {
            const proto::router::StunServerInfo& stun_info = offer.stun_info();
            if (stun_info.version() != 1)
            {
                LOG(INFO) << "Unsupported stun server version";
            }
            else
            {
                connection.stun_host = QString::fromStdString(stun_info.host());
                connection.stun_port = stun_info.port();

                // An empty string means that the router address should be used.
                if (connection.stun_host.isEmpty())
                    connection.stun_host = router_address_.host();
            }
        }

        channels_.emplace_back(std::move(connection));
    }

    emit sig_clientConnected();
}

//--------------------------------------------------------------------------------------------------
void RouterManager::onTimer(TimePoint now)
{
    if (tcp_channel_)
        tcp_channel_->tick(now);

    if (now >= password_expire_time_)
        onNewOneTimePassword();

    if (now >= reconnect_time_)
    {
        reconnect_time_ = TimePoint::max();
        connectToRouter();
    }

    for (auto it = pending_connection_keys_.begin(); it != pending_connection_keys_.end();)
    {
        if (now >= it->second.deadline)
        {
            LOG(INFO) << "Connection key" << it->first << "expired";
            it = pending_connection_keys_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void RouterManager::connectToRouter()
{
    LOG(INFO) << "Connecting to router...";

    if (tcp_channel_)
    {
        LOG(ERROR) << "TCP channel is already connected";
        return;
    }

    ClientAuthenticator* authenticator = new ClientAuthenticator();

    authenticator->setIdentify(proto::key_exchange::IDENTIFY_ANONYMOUS);
    authenticator->setPeerPublicKey(public_key_);
    authenticator->setSessionType(proto::router::SESSION_TYPE_HOST);

    tcp_channel_ = new TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &TcpChannel::sig_authenticated, this, &RouterManager::onTcpReady);
    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &RouterManager::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &RouterManager::onTcpMessageReceived);

    routerStateChanged(proto::user::RouterState::CONNECTING);
    tcp_channel_->connectTo(router_address_.host(), router_address_.port());
}

//--------------------------------------------------------------------------------------------------
void RouterManager::delayedConnectToRouter()
{
    LOG(INFO) << "Reconnect after" << kReconnectTimeout.count() << "seconds";
    reconnect_time_ = Clock::now() + kReconnectTimeout;
}

//--------------------------------------------------------------------------------------------------
void RouterManager::routerStateChanged(proto::user::RouterState::State state)
{
    LOG(INFO) << "Router state changed:" << state;

    router_state_.set_state(state);
    router_state_.set_host_name(router_address_.host().toStdString());
    router_state_.set_host_port(router_address_.port());

    emit sig_routerStateChanged(router_state_);
}

//--------------------------------------------------------------------------------------------------
void RouterManager::hostIdRequest()
{
    LOG(INFO) << "Started ID request for session";

    if (!tcp_channel_ || !tcp_channel_->isAuthenticated())
    {
        LOG(INFO) << "No active connection to the router";
        return;
    }

    HostStorage host_key_storage;
    QByteArray host_key = host_key_storage.hostKey();

    proto::router::HostToRouter message;
    proto::router::HostIdRequest* host_id_request = message.mutable_host_id_request();
    host_id_request->set_hw_id(SysInfo::hardwareId().toStdString());

    if (host_key.isEmpty())
    {
        LOG(INFO) << "Host key is empty. Request for a new ID";
        host_id_request->set_type(proto::router::HostIdRequest::NEW_ID);
    }
    else
    {
        LOG(INFO) << "Host key is present. Request for an existing ID";
        host_id_request->set_type(proto::router::HostIdRequest::EXISTING_ID);
        host_id_request->set_key(host_key.toStdString());
    }

    // Send host ID request.
    LOG(INFO) << "Send ID request to router";
    tcp_channel_->send(0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void RouterManager::readConnectionKeyRequest(const proto::router::ConnectionKeyRequest& request)
{
    proto::router::HostToRouter out_message;
    proto::router::ConnectionKeyResponse* response = out_message.mutable_connection_key_response();
    response->set_request_id(request.request_id());

    const QString user_name = QString::fromStdString(request.user_name());
    const BitSet<quint32> session_type(request.session_type());

    if (!User::isValidUserName(user_name) || session_type.count() != 1)
    {
        LOG(ERROR) << "Invalid connection key request from router";
        response->set_error_code(proto::router::kErrorInvalidData);
    }
    else if (pending_connection_keys_.size() >= kMaxPendingConnectionKeys)
    {
        LOG(ERROR) << "Too many outstanding connection keys. Refusing the request for"
                   << user_name;
        response->set_error_code(proto::router::kErrorInternalError);
    }
    else
    {
        KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
        const QByteArray public_key = key_pair.isValid() ? key_pair.publicKey() : QByteArray();

        if (public_key.isEmpty())
        {
            LOG(ERROR) << "Failed to generate a connection key";
            response->set_error_code(proto::router::kErrorInternalError);
        }
        else
        {
            const quint32 key_id = next_connection_key_id_++;

            PendingConnectionKey& pending = pending_connection_keys_[key_id];
            pending.key_pair = std::move(key_pair);
            pending.user_name = user_name;
            pending.session_type = request.session_type();
            pending.deadline = Clock::now() + kConnectionKeyTtl;

            LOG(INFO) << "Connection key" << key_id << "issued for" << user_name
                      << "(session type:" << request.session_type() << ")";

            response->set_error_code(proto::router::kErrorOk);
            response->set_key_id(key_id);
            response->set_host_public_key(public_key.toStdString());
        }
    }

    tcp_channel_->send(0, serialize(out_message));
}

//--------------------------------------------------------------------------------------------------
void RouterManager::readConnectionOffer(const proto::router::ConnectionOffer& offer)
{
    LOG(INFO) << "New connection offer";

    if (offer.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Invalid connection offer";
        return;
    }

    ScopedQPointer<ServerAuthenticator> authenticator(new ServerAuthenticator());

    if (!offer.host_key_id())
    {
        // An offer without a key runs the password handshake.
        authenticator->setUserList(user_list_);
    }
    else
    {
        const auto it = pending_connection_keys_.find(offer.host_key_id());
        if (it == pending_connection_keys_.end())
        {
            // Spent, expired or issued over a previous channel. Without the pair the client
            // cannot be authenticated, and the password path was not offered to it either.
            LOG(ERROR) << "No pair for connection key" << offer.host_key_id()
                       << ". The offer is dropped";
            return;
        }

        const PendingConnectionKey pending = std::move(it->second);
        pending_connection_keys_.erase(it);

        if (!authenticator->setPrivateKey(pending.key_pair.privateKey()) ||
            !authenticator->setAnonymousAccess(ServerAuthenticator::AnonymousAccess::ENABLE,
                                               pending.session_type))
        {
            LOG(ERROR) << "Failed to load connection key" << offer.host_key_id()
                       << "into the authenticator. The offer is dropped";
            return;
        }

        LOG(INFO) << "Connection key" << offer.host_key_id() << "is taken by a connection of"
                  << pending.user_name;
    }

    peer_manager_->addConnectionOffer(offer, authenticator.release());
}

//--------------------------------------------------------------------------------------------------
void RouterManager::renewOneTimePassword()
{
    PasswordGenerator generator;
    generator.setCharacters(database_.oneTimePasswordCharacters());
    generator.setLength(database_.oneTimePasswordLength());

    one_time_password_ = generator.result();

    MilliSeconds expire_interval = database_.oneTimePasswordExpire();
    if (expire_interval > MilliSeconds(0))
        password_expire_time_ = Clock::now() + expire_interval;
    else
        password_expire_time_ = TimePoint::max();

    user_list_->setOneTimeUser(createOneTimeUser());
}

//--------------------------------------------------------------------------------------------------
User RouterManager::createOneTimeUser() const
{
    if (host_id_ == kInvalidHostId)
    {
        LOG(INFO) << "Invalid host ID";
        return User();
    }

    if (one_time_password_.isEmpty())
    {
        LOG(INFO) << "No password for one-time user";
        return User();
    }

    QString username = '#' + hostIdToString(host_id_);
    User user = User::create(username, one_time_password_);

    user.sessions = one_time_sessions_;
    user.flags = User::ENABLED;

    LOG(INFO) << "One time user" << username << "created (host_id:" << host_id_
              << "sessions:" << one_time_sessions_ << ")";
    return user;
}
