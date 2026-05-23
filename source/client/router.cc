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

#include "client/router.h"

#include <QHash>
#include <QTimer>

#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "build/build_config.h"
#include "proto/key_exchange.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

const std::chrono::seconds kReconnectTimeout{ 5 };
constexpr int kGroupKeySize = 32;

//--------------------------------------------------------------------------------------------------
QHash<qint64, Router*>& instances()
{
    static thread_local QHash<qint64, Router*> g_instances;
    return g_instances;
}

//--------------------------------------------------------------------------------------------------
QByteArray encryptWithGroupKey(const QString& plaintext, const SecureByteArray& gk)
{
    if (plaintext.isEmpty() || gk.isEmpty())
        return QByteArray();

    DataCryptor cryptor(gk);
    std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext.toUtf8());
    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Failed to encrypt with group key";
        return QByteArray();
    }

    return *encrypted;
}

//--------------------------------------------------------------------------------------------------
QString decryptWithGroupKey(std::string_view ciphertext, const SecureByteArray& gk)
{
    if (ciphertext.empty() || gk.isEmpty())
        return QString();

    DataCryptor cryptor(gk);
    std::optional<QByteArray> decrypted = cryptor.decrypt(
        QByteArrayView(ciphertext.data(), static_cast<qsizetype>(ciphertext.size())));
    if (!decrypted.has_value())
    {
        LOG(ERROR) << "Failed to decrypt with group key";
        return QString();
    }

    return QString::fromUtf8(*decrypted);
}

struct Registrator
{
    Registrator()
    {
        qRegisterMetaType<Router::Workspace>("Router::Workspace");
        qRegisterMetaType<Router::WorkspaceList>("Router::WorkspaceList");
    }
};

static volatile Registrator registrator;

} // namespace

//--------------------------------------------------------------------------------------------------
Router::Router(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Reconnecting to router" << config_.address();
        onConnectToRouter();
    });
}

//--------------------------------------------------------------------------------------------------
Router::~Router()
{
    LOG(INFO) << "Dtor";
    instances().remove(config_.routerId());
    onDisconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
// static
Router* Router::instance(qint64 router_id)
{
    if (!instances().contains(router_id))
        return nullptr;

    return instances().value(router_id);
}

//--------------------------------------------------------------------------------------------------
void Router::onConnectToRouter()
{
    // We cannot perform registration in the constructor because the constructor is executed in the
    // GUI thread.
    instances().insert(config_.routerId(), this);

    reconnect_timer_->stop();

    if (status_ != Status::OFFLINE)
        return;

    LOG(INFO) << "Connecting to router" << config_.address();

    setStatus(Status::CONNECTING);

    ClientAuthenticator* authenticator = new ClientAuthenticator(this);
    authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
    authenticator->setSessionType(config_.sessionType());
    authenticator->setUserName(config_.username());
    authenticator->setPassword(SecureString(config_.password()));

    tcp_channel_ = new TcpChannelNG(authenticator, this);

    connect(tcp_channel_, &TcpChannel::sig_authenticated,
            this, &Router::onTcpReady);
    connect(tcp_channel_, &TcpChannel::sig_errorOccurred,
            this, &Router::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived,
            this, &Router::onTcpMessageReceived);

    Address address = Address::fromString(config_.address(), DEFAULT_ROUTER_TCP_PORT);
    tcp_channel_->connectTo(address.host(), address.port());
}

//--------------------------------------------------------------------------------------------------
void Router::onDisconnectFromRouter()
{
    reconnect_timer_->stop();

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_.reset();
    }

    user_id_ = 0;
    user_private_key_.clear();
    workspace_group_keys_.clear();

    setStatus(Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
const RouterConfig& Router::config() const
{
    return config_;
}

//--------------------------------------------------------------------------------------------------
qint64 Router::routerId() const
{
    return config_.routerId();
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Router::version() const
{
    if (!tcp_channel_)
        return QVersionNumber();

    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
void Router::onUpdateConfig(const RouterConfig& config)
{
    bool need_reconnect = !config_.hasSameParams(config);
    config_ = config;

    if (need_reconnect)
    {
        onDisconnectFromRouter();
        onConnectToRouter();
    }
}

//--------------------------------------------------------------------------------------------------
void Router::onRelayListRequest()
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::RelayListRequest* request = message.mutable_relay_list_request();
    request->set_dummy(1);

    LOG(INFO) << "Sending relay list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onClientListRequest()
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::ClientListRequest* request = message.mutable_client_list_request();
    request->set_dummy(1);

    LOG(INFO) << "Sending client list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onUserListRequest()
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    message.mutable_user_list_request()->set_dummy(1);

    LOG(INFO) << "Sending user list request";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onAddUser(const proto::router::User& user)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_command_name(proto::router::kCommandUserAdd);
    request->mutable_user()->CopyFrom(user);

    LOG(INFO) << "Sending user add request (username:" << user.name()
              << ", entry_id:" << user.entry_id() << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onModifyUser(const proto::router::User& user)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_command_name(proto::router::kCommandUserModify);
    request->mutable_user()->CopyFrom(user);

    LOG(INFO) << "Sending user modify request (username:" << user.name()
              << ", entry_id:" << user.entry_id() << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDeleteUser(qint64 entry_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::UserRequest* request = message.mutable_user_request();
    request->set_command_name(proto::router::kCommandUserDelete);
    request->mutable_user()->set_entry_id(entry_id);

    LOG(INFO) << "Sending user delete request (entry_id:" << entry_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDisconnectHost(qint64 host_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostRequest* request = message.mutable_host_request();
    request->set_command_name(proto::router::kCommandHostDisconnect);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending host disconnect request (host_id:" << host_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onRemoveHost(qint64 host_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::HostRequest* request = message.mutable_host_request();
    request->set_command_name(proto::router::kCommandHostRemove);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending host remove request (host_id:" << host_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDisconnectRelay(qint64 session_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::RelayRequest* request = message.mutable_relay_request();
    request->set_command_name(proto::router::kCommandRelayDisconnect);
    request->set_entry_id(session_id);

    LOG(INFO) << "Sending relay disconnect request (entry_id:" << session_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDisconnectClient(qint64 session_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::ClientRequest* request = message.mutable_client_request();
    request->set_command_name(proto::router::kCommandClientDisconnect);
    request->set_entry_id(session_id);

    LOG(INFO) << "Sending client disconnect request (entry_id:" << session_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDisconnectPeer(qint64 relay_entry_id, quint64 peer_session_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
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
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onWorkspaceListRequest()
{
    proto::router::ClientToRouter message;
    message.mutable_workspace_list_request()->set_dummy(1);

    LOG(INFO) << "Sending workspace list request";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onAddWorkspace(const Router::Workspace& workspace)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::Workspace out;
    if (!buildWorkspace(workspace, &out))
        return;

    proto::router::AdminToRouter message;
    proto::router::WorkspaceRequest* request = message.mutable_workspace_request();
    request->set_command_name(proto::router::kCommandWorkspaceAdd);
    request->mutable_workspace()->Swap(&out);

    LOG(INFO) << "Sending workspace add request (name:" << workspace.name << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onModifyWorkspace(const Router::Workspace& workspace)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::Workspace out;
    if (!buildWorkspace(workspace, &out))
        return;

    proto::router::AdminToRouter message;
    proto::router::WorkspaceRequest* request = message.mutable_workspace_request();
    request->set_command_name(proto::router::kCommandWorkspaceModify);
    request->mutable_workspace()->Swap(&out);

    LOG(INFO) << "Sending workspace modify request (entry_id:" << workspace.entry_id
              << ", name:" << workspace.name << ", access entries:" << workspace.access.size() << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onDeleteWorkspace(qint64 entry_id)
{
    if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
    {
        LOG(ERROR) << "No administrator access level";
        return;
    }

    proto::router::AdminToRouter message;
    proto::router::WorkspaceRequest* request = message.mutable_workspace_request();
    request->set_command_name(proto::router::kCommandWorkspaceDelete);
    request->mutable_workspace()->set_entry_id(entry_id);

    LOG(INFO) << "Sending workspace delete request (entry_id:" << entry_id << ")";
    sendMessage(proto::router::CHANNEL_ID_ADMIN, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onConnectionRequest(qint64 request_id, quint64 host_id)
{
    proto::router::ClientToRouter message;
    proto::router::ConnectionRequest* request = message.mutable_connection_request();
    request->set_request_id(request_id);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending connection request:" << *request;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onCheckHostStatus(qint64 request_id, quint64 host_id)
{
    proto::router::ClientToRouter message;
    proto::router::CheckHostStatus* request = message.mutable_check_host_status();
    request->set_request_id(request_id);
    request->set_host_id(host_id);

    LOG(INFO) << "Sending check host status:" << *request;
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onHostListRequest(const proto::router::HostListRequest& request)
{
    proto::router::ClientToRouter message;
    message.mutable_host_list_request()->CopyFrom(request);

    LOG(INFO) << "Sending host list request (mode:" << request.mode()
              << ", workspace_id:" << request.workspace_id()
              << ", group_id:" << request.group_id() << ")";
    sendMessage(proto::router::CHANNEL_ID_CLIENT, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpReady()
{
    CHECK(tcp_channel_);

    LOG(INFO) << "Connected to router" << config_.address();
    reconnect_timer_->stop();

    // Status stays CONNECTING until the router pushes UserKeys; only then we consider the
    // session usable and allow outgoing requests.
    tcp_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Router connection error:" << error_code;

    if (tcp_channel_)
    {
        tcp_channel_->disconnect();
        tcp_channel_.reset();
    }

    user_id_ = 0;
    user_private_key_.clear();
    workspace_group_keys_.clear();

    emit sig_errorOccurred(config_.routerId(), error_code);

    setStatus(Status::OFFLINE);

    LOG(INFO) << "Reconnect scheduled in" << kReconnectTimeout.count() << "seconds";
    reconnect_timer_->start(kReconnectTimeout);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_ADMIN)
    {
        if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN)
        {
            LOG(ERROR) << "Unexpected message from channel" << channel_id;
            return;
        }

        proto::router::RouterToAdmin message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse admin message";
            return;
        }

        if (message.has_relay_list())
        {
            LOG(INFO) << "Relay list received";
            emit sig_relayListReceived(message.relay_list());
        }
        else if (message.has_client_list())
        {
            LOG(INFO) << "Client list received";
            emit sig_clientListReceived(message.client_list());
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
        else if (message.has_client_result())
        {
            LOG(INFO) << "Client result received";
            emit sig_clientResultReceived(message.client_result());
        }
        else if (message.has_workspace_result())
        {
            LOG(INFO) << "Workspace result received";
            emit sig_workspaceResultReceived(message.workspace_result());
        }
        else
        {
            LOG(WARNING) << "Unhandled admin message";
            return;
        }
    }
    else if (channel_id == proto::router::CHANNEL_ID_MANAGER)
    {
        if (config_.sessionType() != proto::router::SESSION_TYPE_ADMIN &&
            config_.sessionType() != proto::router::SESSION_TYPE_MANAGER)
        {
            LOG(ERROR) << "Unexpected message from channel" << channel_id;
            return;
        }

        // TODO
    }
    else if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        proto::router::RouterToClient message;
        if (!parse(buffer, &message))
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
        else if (message.has_host_list())
        {
            LOG(INFO) << "Host list received";
            emit sig_hostListReceived(message.host_list());
        }
        else if (message.has_user_keys())
        {
            LOG(INFO) << "User keys received (user_id:" << message.user_keys().user_id() << ")";
            readUserKeys(message.user_keys());
            setStatus(Status::ONLINE);
        }
        else if (message.has_workspace_list())
        {
            LOG(INFO) << "Workspace list received";
            readWorkspaceList(message.workspace_list());
        }
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Router::setStatus(Status status)
{
    if (status_ != status)
    {
        LOG(INFO) << "Status changed from" << status_ << "to" << status;
        status_ = status;
        emit sig_statusChanged(config_.routerId(), status_);
    }
}

//--------------------------------------------------------------------------------------------------
void Router::sendMessage(quint8 channel_id, const QByteArray& data)
{
    if (!tcp_channel_)
        return;

    if (status_ != Status::ONLINE)
    {
        LOG(WARNING) << "Dropping outgoing message, router not online yet (status:" << status_ << ")";
        return;
    }

    tcp_channel_->send(channel_id, data);
}

//--------------------------------------------------------------------------------------------------
SecureByteArray Router::unwrapGroupKey(const QByteArray& wrapped_gk) const
{
    if (wrapped_gk.isEmpty() || user_private_key_.isEmpty())
        return SecureByteArray();

    KeyPair key_pair = KeyPair::fromPrivateKey(user_private_key_);
    if (!key_pair.isValid())
    {
        LOG(ERROR) << "Failed to load key pair from private key";
        return SecureByteArray();
    }

    std::optional<QByteArray> opened = SealedBox::open(wrapped_gk, key_pair);
    if (!opened.has_value() || opened->isEmpty())
    {
        LOG(ERROR) << "Failed to open sealed group key";
        return SecureByteArray();
    }

    return SecureByteArray(std::move(*opened));
}

//--------------------------------------------------------------------------------------------------
void Router::readUserKeys(const proto::router::UserKeys& user_keys)
{
    user_id_ = user_keys.user_id();

    const QByteArray wrap_private_key = QByteArray::fromStdString(user_keys.wrap_private_key());
    const QByteArray wrap_salt = QByteArray::fromStdString(user_keys.wrap_salt());

    if (wrap_private_key.isEmpty() || wrap_salt.isEmpty())
    {
        LOG(WARNING) << "UserKeys missing wrap key/salt; crypto features unavailable";
        user_private_key_.clear();
        return;
    }

    user_private_key_ = PrivateKeyCryptor::decrypt(wrap_private_key, config_.password(), wrap_salt);
    if (user_private_key_.isEmpty())
        LOG(WARNING) << "Failed to decrypt self private key for user_id:" << user_id_;
}

//--------------------------------------------------------------------------------------------------
void Router::readWorkspaceList(const proto::router::WorkspaceList& list)
{
    workspace_group_keys_.clear();

    Router::WorkspaceList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.workspaces.reserve(list.workspace_size());

    for (int i = 0; i < list.workspace_size(); ++i)
    {
        const proto::router::Workspace& src = list.workspace(i);

        Router::Workspace& dst = decoded.workspaces.emplaceBack();
        dst.entry_id = src.entry_id();
        dst.name     = QString::fromStdString(src.name());
        dst.access.reserve(src.access_size());

        QByteArray self_wrapped_gk;
        for (int j = 0; j < src.access_size(); ++j)
        {
            const proto::router::WorkspaceAccess& access = src.access(j);
            dst.access.emplaceBack().user_id = access.user_id();

            if (access.user_id() == user_id_)
                self_wrapped_gk = QByteArray::fromStdString(access.wrapped_gk());
        }

        if (!self_wrapped_gk.isEmpty())
        {
            SecureByteArray gk = unwrapGroupKey(self_wrapped_gk);
            if (!gk.isEmpty())
            {
                if (!src.comment().empty())
                    dst.comment = decryptWithGroupKey(src.comment(), gk);

                workspace_group_keys_.insert(src.entry_id(), std::move(gk));
            }
        }
    }

    emit sig_workspaceListReceived(decoded);
}

//--------------------------------------------------------------------------------------------------
bool Router::buildWorkspace(const Workspace& workspace, proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    out->set_name(workspace.name.toStdString());

    // Detect whether we need a workspace group key: required when adding new users (to seal it
    // for them) and/or when the comment is non-empty (to encrypt it).
    bool has_added = false;
    for (const auto& access : workspace.access)
    {
        if (!access.public_key.isEmpty())
        {
            has_added = true;
            break;
        }
    }

    SecureByteArray group_key;
    const bool needs_gk = has_added || !workspace.comment.isEmpty();
    if (needs_gk)
    {
        if (user_private_key_.isEmpty())
        {
            LOG(ERROR) << "User private key unavailable";
            return false;
        }

        auto it = workspace_group_keys_.constFind(workspace.entry_id);
        if (it == workspace_group_keys_.constEnd())
        {
            // Bootstrap: no workspace GK known to us (either a new workspace or none of the
            // existing access entries belong to us). Generate a fresh GK to seal for the new
            // grantees.
            group_key = SecureByteArray(Random::byteArray(kGroupKeySize));
        }
        else
        {
            group_key = *it;
        }
    }

    if (!workspace.comment.isEmpty())
    {
        QByteArray encrypted = encryptWithGroupKey(workspace.comment, group_key);
        if (encrypted.isEmpty())
        {
            LOG(ERROR) << "Failed to encrypt comment";
            return false;
        }
        out->set_comment(encrypted.toStdString());
    }

    for (const auto& access : workspace.access)
    {
        proto::router::WorkspaceAccess* dst = out->add_access();
        dst->set_user_id(access.user_id);

        if (access.public_key.isEmpty())
            continue; // Existing user - server preserves their wrapped_gk.

        QByteArray wrapped_gk = SealedBox::seal(group_key.toByteArray(), access.public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to seal group key for user_id:" << access.user_id;
            return false;
        }
        dst->set_wrapped_gk(wrapped_gk.toStdString());
    }

    return true;
}
