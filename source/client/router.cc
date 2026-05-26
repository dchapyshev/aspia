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
#include "base/crypto/data_cryptor.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/net/address.h"
#include "base/net/tcp_channel_ng.h"
#include "base/peer/client_authenticator.h"
#include "proto/key_exchange.h"

namespace {

constexpr int kGroupKeySize = 32;
const std::chrono::seconds kReconnectTimeout { 5 };

//--------------------------------------------------------------------------------------------------
struct Registrator
{
    Registrator()
    {
        qRegisterMetaType<Router::Workspace>("Router::Workspace");
        qRegisterMetaType<Router::WorkspaceList>("Router::WorkspaceList");
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
        LOG(INFO) << "Reconnect scheduled in" << kReconnectTimeout.count() << "seconds";
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
    else if (channel_id == proto::router::CHANNEL_ID_CLIENT)
    {
        proto::router::RouterToClient message;
        if (!parse(bytes, &message))
        {
            LOG(ERROR) << "Unable to parse client message";
            return;
        }

        if (message.has_user_keys())
            readUserKeys(message.user_keys());
        else if (message.has_connection_offer())
            dispatch(message.connection_offer().request_id(), message.connection_offer());
        else if (message.has_host_status())
            dispatch(message.host_status().request_id(), message.host_status());
        else if (message.has_host_list())
            dispatch(message.host_list().request_id(), message.host_list());
        else if (message.has_workspace_list())
            dispatch(message.workspace_list().request_id(), message.workspace_list());
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
void Router::setStatus(Status status)
{
    if (status_ == status)
        return;
    status_ = status;
    emit sig_statusChanged(config_.routerId(), status_);
}

//--------------------------------------------------------------------------------------------------
bool Router::buildWorkspace(const Router::Workspace& workspace, proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    out->set_name(workspace.name.toStdString());

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
            group_key = SecureByteArray(Random::byteArray(kGroupKeySize));
        else
            group_key = *it;
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

    for (HostId host_id : std::as_const(workspace.host_ids))
        out->add_host_id(host_id);

    return true;
}

//--------------------------------------------------------------------------------------------------
void Router::setupChannel()
{
    if (tcp_channel_)
        return;

    LOG(INFO) << "Connecting to router" << config_.address();

    Address address = Address::fromString(config_.address(), DEFAULT_ROUTER_TCP_PORT);

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
    workspace_group_keys_.clear();
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

    setStatus(Status::ONLINE);
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
}

//--------------------------------------------------------------------------------------------------
Router::WorkspaceList Router::decodeWorkspaceList(const proto::router::WorkspaceList& list)
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

    std::optional<QByteArray> opened = SealedBox::open(wrapped_gk, key_pair);
    if (!opened.has_value() || opened->isEmpty())
    {
        LOG(ERROR) << "Failed to open sealed group key";
        return SecureByteArray();
    }

    return SecureByteArray(std::move(*opened));
}
