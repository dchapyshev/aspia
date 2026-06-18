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

#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "client/database.h"
#include "proto/key_exchange.h"

namespace {

constexpr int kGroupKeySize = 32;
const std::chrono::milliseconds kReconnectTimeout { 2500 };

//--------------------------------------------------------------------------------------------------
struct Registrator
{
    Registrator()
    {
        qRegisterMetaType<Router::Workspace>("Router::Workspace");
        qRegisterMetaType<Router::WorkspaceList>("Router::WorkspaceList");
        qRegisterMetaType<Router::Host>("Router::Host");
        qRegisterMetaType<Router::HostList>("Router::HostList");
        qRegisterMetaType<Router::Group>("Router::Group");
        qRegisterMetaType<Router::GroupList>("Router::GroupList");
    }
};

static volatile Registrator registrator;

//--------------------------------------------------------------------------------------------------
QHash<qint64, Router*>& instances()
{
    static thread_local QHash<qint64, Router*> g_instances;
    return g_instances;
}

//--------------------------------------------------------------------------------------------------
QByteArray encrypt(const DataCryptor& cryptor, const QString& plaintext)
{
    if (plaintext.isEmpty())
        return QByteArray();

    std::optional<QByteArray> encrypted = cryptor.encrypt(plaintext.toUtf8());
    if (!encrypted.has_value())
    {
        LOG(ERROR) << "Failed to encrypt with group key";
        return QByteArray();
    }

    return *encrypted;
}

//--------------------------------------------------------------------------------------------------
QString decrypt(const DataCryptor& cryptor, std::string_view ciphertext)
{
    if (ciphertext.empty())
        return QString();

    std::optional<QByteArray> decrypted = cryptor.decrypt(
        QByteArrayView(ciphertext.data(), static_cast<qsizetype>(ciphertext.size())));
    if (!decrypted.has_value())
    {
        LOG(ERROR) << "Failed to decrypt with group key";
        return QString();
    }

    return QString::fromUtf8(*decrypted);
}

//--------------------------------------------------------------------------------------------------
// Re-seal every workspace key in |cryptors| to |new_public_key| and append the results to
// |message| (any proto with a repeated WorkspaceKey workspace_key field).
template<typename MessageT>
void resealWorkspaceKeys(const std::unordered_map<qint64, DataCryptor>& cryptors,
                         const QByteArray& new_public_key, MessageT* message)
{
    for (const auto& [workspace_id, cryptor] : cryptors)
    {
        QByteArray wrapped_gk = SealedBox::seal(cryptor.key(), new_public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to reseal group key for workspace" << workspace_id;
            continue;
        }

        auto* dst = message->add_workspace_key();
        dst->set_workspace_id(workspace_id);
        dst->set_wrapped_gk(wrapped_gk.toStdString());
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
Router::Router(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config),
      reconnect_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    instances().insert(config_.routerId(), this);

    reconnect_timer_->setSingleShot(true);
    connect(reconnect_timer_, &QTimer::timeout, this, &Router::onReconnectTimeout);

    connect(GuiApplication::instance(), &QGuiApplication::applicationStateChanged,
            this, &Router::onApplicationStateChanged);
}

//--------------------------------------------------------------------------------------------------
Router::~Router()
{
    LOG(INFO) << "Dtor";
    instances().remove(config_.routerId());
}

//--------------------------------------------------------------------------------------------------
// static
Router* Router::instance(qint64 router_id)
{
    return instances().value(router_id);
}

//--------------------------------------------------------------------------------------------------
void Router::connectToRouter()
{
    setStatus(Status::CONNECTING);
    setupChannel();
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectFromRouter()
{
    reconnect_timer_->stop();
    destroyChannel();
    setStatus(Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
void Router::updateConfig(const RouterConfig& config)
{
    const bool need_reconnect = !config_.hasSameParams(config);
    config_ = config;

    if (need_reconnect && status_ != Status::OFFLINE)
    {
        destroyChannel();
        setStatus(Status::CONNECTING);
        setupChannel();
    }
}

//--------------------------------------------------------------------------------------------------
void Router::submitTwoFactorCode(const QString& totp_code)
{
    proto::router::ClientToRouter message;
    proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
    response->set_totp_code(totp_code.toStdString());
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpAuthenticated()
{
    if (!tcp_channel_)
        return;

    LOG(INFO) << "Connected to router" << config_.address();
    reconnect_timer_->stop();
    version_ = tcp_channel_->peerVersion();

    QMetaObject::invokeMethod(tcp_channel_, &TcpChannel::setPaused, Qt::QueuedConnection, false);
    // Stay in CONNECTING; transition to ONLINE happens when UserKeys arrives.
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Router connection error:" << error_code;

    destroyChannel();

    // Schedule auto-reconnect unless the user has explicitly torn the session down.
    if (status_ != Status::OFFLINE)
    {
        setStatus(Status::CONNECTING);
        LOG(INFO) << "Reconnect scheduled in" << kReconnectTimeout.count() << "ms";
        reconnect_timer_->start(kReconnectTimeout);
    }

    emit sig_errorOccurred(config_.routerId(), error_code);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpMessageReceived(quint8 channel_id, const QByteArray& bytes)
{
    if (channel_id == proto::router::CHANNEL_ID_ADMIN)
    {
        proto::router::RouterToAdmin message;
        if (!parse(bytes, &message))
        {
            LOG(ERROR) << "Unable to parse admin message";
            return;
        }

        if (message.has_relay_list())
            dispatch(message.relay_list().request_id(), message.relay_list());
        else if (message.has_client_list())
            dispatch(message.client_list().request_id(), message.client_list());
        else if (message.has_user_list())
            dispatch(message.user_list().request_id(), message.user_list());
        else if (message.has_user_result())
            dispatch(message.user_result().request_id(), message.user_result());
        else if (message.has_host_result())
            dispatch(message.host_result().request_id(), message.host_result());
        else if (message.has_relay_result())
            dispatch(message.relay_result().request_id(), message.relay_result());
        else if (message.has_client_result())
            dispatch(message.client_result().request_id(), message.client_result());
        else if (message.has_workspace_result())
            dispatch(message.workspace_result().request_id(), message.workspace_result());
        else if (message.has_peer_result())
            dispatch(message.peer_result().request_id(), message.peer_result());
        else
            LOG(WARNING) << "Unhandled admin message";
    }
    else if (channel_id == proto::router::CHANNEL_ID_MANAGER)
    {
        proto::router::RouterToManager message;
        if (!parse(bytes, &message))
        {
            LOG(ERROR) << "Unable to parse manager message";
            return;
        }

        if (message.has_host_result())
            dispatch(message.host_result().request_id(), message.host_result());
        else if (message.has_group_result())
            dispatch(message.group_result().request_id(), message.group_result());
        else
            LOG(WARNING) << "Unhandled manager message";
    }
    else if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        proto::router::RouterToClient message;
        if (!parse(bytes, &message))
        {
            LOG(ERROR) << "Unable to parse client message";
            return;
        }

        if (message.has_two_factor_challenge())
            readTwoFactorChallenge(message.two_factor_challenge());
        else if (message.has_two_factor_result())
            readTwoFactorResult(message.two_factor_result());
        else if (message.has_user_keys())
            readUserKeys(message.user_keys());
        else if (message.has_connection_offer())
            dispatch(message.connection_offer().request_id(), message.connection_offer());
        else if (message.has_host_status())
            dispatch(message.host_status().request_id(), message.host_status());
        else if (message.has_host_list())
            dispatch(message.host_list().request_id(), message.host_list());
        else if (message.has_host_search_result())
            dispatch(message.host_search_result().request_id(), message.host_search_result());
        else if (message.has_workspace_list())
            dispatch(message.workspace_list().request_id(), message.workspace_list());
        else if (message.has_group_list())
            dispatch(message.group_list().request_id(), message.group_list());
        else if (message.has_change_password_result())
            dispatch(message.change_password_result().request_id(), message.change_password_result());
        else if (message.has_notification())
            emitNotificationSignals(message.notification());
        else
            LOG(WARNING) << "Unhandled client message";
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Router::onReconnectTimeout()
{
    if (status_ == Status::OFFLINE)
        return;
    LOG(INFO) << "Reconnecting to router" << config_.address();
    setupChannel();
}

//--------------------------------------------------------------------------------------------------
void Router::onApplicationStateChanged(Qt::ApplicationState state)
{
#if defined(Q_OS_ANDROID)
    // On Android the OS freezes the process in the background and the connection is dropped.
    if (state == Qt::ApplicationActive && status_ == Status::CONNECTING && !tcp_channel_)
    {
        LOG(INFO) << "Reconnecting to router on returning to foreground";
        reconnect_timer_->stop();
        setupChannel();
    }
#else
    Q_UNUSED(state)
#endif
}

//--------------------------------------------------------------------------------------------------
void Router::setStatus(Status status)
{
    if (status_ == status)
        return;
    status_ = status;

    if (status_ != Status::ONLINE)
    {
        workspaces_loaded_ = false;
        cached_workspaces_.clear();
        cached_groups_.clear();
        cached_hosts_.clear();
        pending_.clear();
    }

    emit sig_statusChanged(config_.routerId(), status_);
}

//--------------------------------------------------------------------------------------------------
bool Router::buildWorkspace(const Router::Workspace& workspace, proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    out->set_name(workspace.name.toStdString());

    if (user_private_key_.isEmpty())
    {
        LOG(ERROR) << "User private key unavailable";
        return false;
    }

    SecureByteArray group_key;

    auto it = workspace_cryptors_.find(workspace.entry_id);
    if (it == workspace_cryptors_.end())
    {
        group_key = SecureByteArray(Random::byteArray(kGroupKeySize));
        it = workspace_cryptors_.emplace(workspace.entry_id,
            DataCryptor(CipherType::AES256_GCM, group_key)).first;
    }
    else
    {
        group_key = it->second.key();
    }

    out->set_comment(encrypt(it->second, workspace.comment).toStdString());

    for (const auto& access : workspace.access)
    {
        proto::router::WorkspaceAccess* dst = out->add_access();
        dst->set_user_id(access.user_id);

        if (access.public_key.isEmpty())
            continue; // Existing user - server preserves their wrapped_gk.

        QByteArray wrapped_gk = SealedBox::seal(group_key, access.public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to seal group key for user_id:" << access.user_id;
            return false;
        }
        dst->set_wrapped_gk(wrapped_gk.toStdString());
    }

    for (HostId host_id : std::as_const(workspace.host_ids))
        out->add_host_id(host_id);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Router::buildHost(const Router::Host& host, proto::router::Host* out)
{
    CHECK(out);

    auto it = workspace_cryptors_.find(host.workspace_id);
    if (it == workspace_cryptors_.end())
    {
        LOG(ERROR) << "No cached cryptor for workspace" << host.workspace_id;
        return false;
    }

    out->set_host_id(host.host_id);
    out->set_group_id(host.group_id);
    out->set_display_name(host.display_name.toStdString());

    const DataCryptor& cryptor = it->second;
    out->set_comment(encrypt(cryptor, host.comment).toStdString());
    out->set_user_name(encrypt(cryptor, host.user_name).toStdString());
    out->set_password(encrypt(cryptor, host.password.toString()).toStdString());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Router::buildGroup(qint64 workspace_id, const Router::Group& group, proto::router::Group* out)
{
    CHECK(out);

    if (group.entry_id > 0)
        out->set_entry_id(group.entry_id);
    out->set_parent_id(group.parent_id);
    out->set_name(group.name.toStdString());

    auto it = workspace_cryptors_.find(workspace_id);
    if (it == workspace_cryptors_.end())
    {
        LOG(ERROR) << "No cached cryptor for workspace" << workspace_id;
        return false;
    }

    out->set_comment(encrypt(it->second, group.comment).toStdString());
    return true;
}

//--------------------------------------------------------------------------------------------------
void Router::setupChannel()
{
    if (tcp_channel_)
        return;

    LOG(INFO) << "Connecting to router" << config_.address();

    Address address = Address::fromString(config_.address(), DEFAULT_ROUTER_CLIENT_TCP_PORT);

    // TcpChannelNG (and its underlying socket) must be constructed in the thread where it
    // will live. Hop into the IO thread via a scratch QObject, build everything there, and
    // block until the channel pointer is ready - construction is microseconds, connectTo()
    // only initiates the async connect. The scratch object is cleaned up via deleteLater()
    // by ScopedQPointer when it goes out of scope.
    ScopedQPointer<QObject> io_bridge(new QObject());
    io_bridge->moveToThread(GuiApplication::ioThread());

    TcpChannel* channel = nullptr;
    QMetaObject::invokeMethod(io_bridge, [this, &channel, host = address.host(), port = address.port()]()
    {
        auto* authenticator = new ClientAuthenticator();
        authenticator->setIdentify(proto::key_exchange::IDENTIFY_SRP);
        authenticator->setSessionType(config_.sessionType());
        authenticator->setUserName(config_.username());
        authenticator->setPassword(config_.password());

        TcpChannel* tcp_channel = new TcpChannelNG(authenticator, nullptr);
        authenticator->setParent(tcp_channel);

        connect(tcp_channel, &TcpChannel::sig_authenticated,
                this, &Router::onTcpAuthenticated, Qt::QueuedConnection);
        connect(tcp_channel, &TcpChannel::sig_errorOccurred,
                this, &Router::onTcpErrorOccurred, Qt::QueuedConnection);
        connect(tcp_channel, &TcpChannel::sig_messageReceived,
                this, &Router::onTcpMessageReceived, Qt::QueuedConnection);

        tcp_channel->connectTo(host, port);
        channel = tcp_channel;
    }, Qt::BlockingQueuedConnection);

    tcp_channel_ = channel;
}

//--------------------------------------------------------------------------------------------------
void Router::destroyChannel()
{
    user_id_ = 0;
    user_private_key_.clear();
    workspace_cryptors_.clear();
    pending_.clear();
    version_ = QVersionNumber();
    tcp_channel_.reset();
}

//--------------------------------------------------------------------------------------------------
void Router::emitSend(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    if (!tcp_channel_)
    {
        LOG(WARNING) << "Dropping outgoing message, channel not ready";
        return;
    }

    QMetaObject::invokeMethod(tcp_channel_, &TcpChannel::send, Qt::QueuedConnection,
                              channel_id, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::readUserKeys(const proto::router::UserKeys& user_keys)
{
    LOG(INFO) << "User keys received (user_id:" << user_keys.user_id() << ")";

    user_id_   = user_keys.user_id();
    user_name_ = QString::fromStdString(user_keys.name());

    const QByteArray wrap_private_key = QByteArray::fromStdString(user_keys.wrap_private_key());
    const QByteArray wrap_salt = QByteArray::fromStdString(user_keys.wrap_salt());

    if (wrap_private_key.isEmpty() || wrap_salt.isEmpty())
    {
        user_private_key_.clear();
        LOG(WARNING) << "User has no wrap key/salt; prompting password change";
        emit sig_passwordChangeRequired(config_.routerId());
        return;
    }

    user_private_key_ = PrivateKeyCryptor::decrypt(
        wrap_private_key, SecureString(config_.password()), wrap_salt);
    if (user_private_key_.isEmpty())
    {
        LOG(WARNING) << "Failed to decrypt self private key for user_id:" << user_id_;
        return;
    }

    workspace_cryptors_.clear();
    for (int i = 0; i < user_keys.workspace_key_size(); ++i)
    {
        const proto::router::UserKeys::WorkspaceKey& wk = user_keys.workspace_key(i);
        SecureByteArray gk = unwrapGroupKey(QByteArray::fromStdString(wk.wrapped_gk()));
        if (gk.isEmpty())
        {
            LOG(WARNING) << "Failed to unwrap GK for workspace" << wk.workspace_id();
            continue;
        }
        workspace_cryptors_.emplace(wk.workspace_id(), DataCryptor(CipherType::AES256_GCM, std::move(gk)));
    }

    setStatus(Status::ONLINE);
}

//--------------------------------------------------------------------------------------------------
void Router::readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge)
{
    switch (challenge.mode())
    {
        case proto::router::TWO_FACTOR_MODE_ACTIVE:
        {
            // The router can ask twice: once at the start of the 2FA stage, and a second
            // time when our presented |token| was rejected (revoked, password change,
            // database wiped). On the latter we must drop the stale local copy and skip
            // straight to the TOTP prompt; otherwise we would loop, presenting the same
            // dead token again.
            if (challenge.token_rejected())
            {
                LOG(INFO) << "Router rejected device token - clearing local copy";
                config_.clearDeviceToken();
                if (!Database::instance().modifyRouter(config_))
                    LOG(WARNING) << "Failed to clear stale device token";

                emit sig_twoFactorCodeRequired(config_.routerId());
                return;
            }

            // Token path: if we hold a token from a previous successful TOTP, present it and
            // skip the prompt entirely. Otherwise ask the UI for a fresh TOTP code.
            const QByteArray token = config_.deviceToken();
            if (!token.isEmpty())
            {
                proto::router::ClientToRouter message;
                proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
                response->set_token(token.toStdString());
                emitSend(proto::router::CHANNEL_ID_CLIENT, message);
                return;
            }

            LOG(INFO) << "Two-factor code required for router" << config_.routerId();
            emit sig_twoFactorCodeRequired(config_.routerId());
            return;
        }

        case proto::router::TWO_FACTOR_MODE_ENROLL:
        {
            const QString uri = QString::fromStdString(challenge.otpauth_uri());

            // Drop any stale token from a previous account life. Enrollment implies a brand
            // new TOTP secret, the old token is dead.
            config_.clearDeviceToken();
            if (!Database::instance().modifyRouter(config_))
                LOG(WARNING) << "Failed to clear stale device token";

            LOG(INFO) << "Two-factor enrollment required for router" << config_.routerId();
            emit sig_twoFactorEnrollment(config_.routerId(), uri);
            return;
        }

        default:
            LOG(WARNING) << "Unknown TwoFactorMode:" << challenge.mode();
            disconnectFromRouter();
            return;
    }
}

//--------------------------------------------------------------------------------------------------
void Router::readTwoFactorResult(const proto::router::TwoFactorResult& result)
{
    // The router sends this only to deliver a freshly issued device token; failures drop the
    // connection instead. Persist the token. Final success is marked separately by UserKeys.
    const QByteArray new_token = QByteArray::fromStdString(result.new_token());
    if (!new_token.isEmpty())
    {
        LOG(INFO) << "Device token issued for router" << config_.routerId();

        config_.setDeviceToken(new_token);
        if (!Database::instance().modifyRouter(config_))
            LOG(WARNING) << "Failed to persist new device token for router" << config_.routerId();
    }
}

//--------------------------------------------------------------------------------------------------
void Router::persistChangedPassword(const SecureString& new_password)
{
    LOG(INFO) << "Password changed for router" << config_.routerId();

    config_.setPassword(new_password);
    if (!Database::instance().modifyRouter(config_))
        LOG(WARNING) << "Failed to persist new password for router" << config_.routerId();
}

//--------------------------------------------------------------------------------------------------
void Router::emitNotificationSignals(const proto::router::Notification& notification)
{
    const qint64 router_id = config_.routerId();

    if (notification.hosts_dirty())
        emit sig_hostsChanged(router_id);
    if (notification.relays_dirty())
        emit sig_relaysChanged(router_id);
    if (notification.clients_dirty())
        emit sig_clientsChanged(router_id);
    if (notification.users_dirty())
        emit sig_usersChanged(router_id);
    if (notification.workspaces_dirty())
        emit sig_workspacesChanged(router_id);
    if (notification.groups_dirty())
        emit sig_groupsChanged(router_id);
}

//--------------------------------------------------------------------------------------------------
Router::WorkspaceList Router::decodeWorkspaceList(const proto::router::WorkspaceList& list)
{
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

        if (self_wrapped_gk.isEmpty())
            continue;

        SecureByteArray gk = unwrapGroupKey(self_wrapped_gk);
        if (gk.isEmpty())
            continue;

        DataCryptor cryptor(CipherType::AES256_GCM, gk);
        if (!src.comment().empty())
            dst.comment = decrypt(cryptor, src.comment());

        workspace_cryptors_.insert_or_assign(src.entry_id(), std::move(cryptor));
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
Router::Host Router::decodeHost(const proto::router::Host& src)
{
    Router::Host dst;
    dst.host_id       = src.host_id();
    dst.workspace_id  = src.workspace_id();
    dst.group_id      = src.group_id();
    dst.display_name  = QString::fromStdString(src.display_name());
    dst.computer_name = QString::fromStdString(src.computer_name());
    dst.cpu_arch      = QString::fromStdString(src.cpu_arch());
    dst.version       = QString::fromStdString(src.version());
    dst.os_name       = QString::fromStdString(src.os_name());
    dst.address       = QString::fromStdString(src.address());
    dst.last_connect  = src.last_connect();
    dst.last_modify   = src.last_modify();
    dst.online        = src.online();

    if (src.workspace_id() == 0)
        return dst;

    auto it = workspace_cryptors_.find(src.workspace_id());
    if (it == workspace_cryptors_.end())
        return dst;

    const DataCryptor& cryptor = it->second;
    if (!src.comment().empty())
        dst.comment = decrypt(cryptor, src.comment());
    if (!src.user_name().empty())
        dst.user_name = decrypt(cryptor, src.user_name());
    if (!src.password().empty())
        dst.password = SecureString(decrypt(cryptor, src.password()));

    return dst;
}

//--------------------------------------------------------------------------------------------------
Router::HostList Router::decodeHostList(const proto::router::HostList& list)
{
    Router::HostList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = list.workspace_id();
    decoded.group_id     = list.group_id();
    decoded.total_count  = list.total_count();
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
        decoded.hosts.append(decodeHost(list.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
Router::HostList Router::decodeHostSearchResult(const proto::router::HostSearchResult& result)
{
    Router::HostList decoded;
    decoded.error_code = QString::fromStdString(result.error_code());
    decoded.hosts.reserve(result.host_size());

    for (int i = 0; i < result.host_size(); ++i)
        decoded.hosts.append(decodeHost(result.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
Router::GroupList Router::decodeGroupList(const proto::router::GroupList& list)
{
    Router::GroupList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = list.workspace_id();
    decoded.groups.reserve(list.group_size());

    for (int i = 0; i < list.group_size(); ++i)
    {
        const proto::router::Group& src = list.group(i);

        Router::Group& dst = decoded.groups.emplaceBack();
        dst.entry_id     = src.entry_id();
        dst.workspace_id = list.workspace_id();
        dst.parent_id    = src.parent_id();
        dst.name         = QString::fromStdString(src.name());

        if (list.workspace_id() == 0)
            continue;

        auto it = workspace_cryptors_.find(list.workspace_id());
        if (it == workspace_cryptors_.end())
            continue;

        const DataCryptor& cryptor = it->second;
        if (!src.comment().empty())
            dst.comment = decrypt(cryptor, src.comment());
    }

    return decoded;
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

    std::optional<SecureByteArray> opened = SealedBox::open(wrapped_gk, key_pair);
    if (!opened.has_value() || opened->isEmpty())
    {
        LOG(ERROR) << "Failed to open sealed group key";
        return SecureByteArray();
    }

    return std::move(*opened);
}

//--------------------------------------------------------------------------------------------------
void Router::resealGroupKeys(const QByteArray& new_public_key, proto::router::User* user)
{
    CHECK(user);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, user);
}

//--------------------------------------------------------------------------------------------------
void Router::resealGroupKeys(const QByteArray& new_public_key,
                             proto::router::ChangePasswordRequest* request)
{
    CHECK(request);
    resealWorkspaceKeys(workspace_cryptors_, new_public_key, request);
}
