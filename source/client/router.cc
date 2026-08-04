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

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/workers/router_worker.h"

namespace {

//--------------------------------------------------------------------------------------------------
struct Registrator
{
    Registrator()
    {
        qRegisterMetaType<Router::Workspace>("Router::Workspace");
        qRegisterMetaType<Router::WorkspaceList>("Router::WorkspaceList");
        qRegisterMetaType<Router::Host>("Router::Host");
        qRegisterMetaType<Router::HostList>("Router::HostList");
        qRegisterMetaType<Router::TempHost>("Router::TempHost");
        qRegisterMetaType<Router::TempHostList>("Router::TempHostList");
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

} // namespace

//--------------------------------------------------------------------------------------------------
Router::Router(const RouterConfig& config, QObject* parent)
    : QObject(parent),
      config_(config)
{
    LOG(INFO) << "Ctor";

    instances().insert(config_.routerId(), this);

    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
    {
        LOG(FATAL) << "Router worker not found";
        return;
    }

    connect(router_worker_, &RouterWorker::sig_authenticated, this, &Router::onTcpAuthenticated,
            Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_errorOccurred, this, &Router::onTcpErrorOccurred,
            Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_messageReceived, this, &Router::onTcpMessageReceived,
            Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
Router::~Router()
{
    LOG(INFO) << "Dtor";
    disconnectWorker();
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
    connectWorker();
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectFromRouter()
{
    disconnectWorker();
    clearSessionState();
    setStatus(Status::OFFLINE);
}

//--------------------------------------------------------------------------------------------------
void Router::updateConfig(const RouterConfig& config)
{
    const bool need_reconnect = !config_.hasSameParams(config);
    config_ = config;

    if (need_reconnect && status_ != Status::OFFLINE)
    {
        disconnectWorker();
        clearSessionState();
        setStatus(Status::CONNECTING);
        connectWorker();
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
void Router::onTcpAuthenticated(qint64 router_id, const QVersionNumber& peer_version)
{
    if (router_id != config_.routerId())
        return;

    LOG(INFO) << "Connected to router" << config_.address();
    version_ = peer_version;
    // The worker already unpaused the channel. Stay in CONNECTING; the transition to ONLINE happens
    // when UserKeys arrives.
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code)
{
    if (router_id != config_.routerId())
        return;

    LOG(INFO) << "Router connection error:" << error_code;
    clearSessionState();

    if (status_ != Status::OFFLINE)
        setStatus(Status::CONNECTING);

    emit sig_errorOccurred(config_.routerId(), error_code);
}

//--------------------------------------------------------------------------------------------------
void Router::onTcpMessageReceived(qint64 router_id, quint8 channel_id, const QByteArray& bytes)
{
    if (router_id != config_.routerId())
        return;

    if (channel_id == proto::router::CHANNEL_ID_ADMIN)
    {
        proto::router::RouterToAdmin message;
        if (!parse(bytes, &message))
        {
            LOG(ERROR) << "Unable to parse admin message";
            return;
        }

        if (message.has_relay_list())
            state_.dispatch(message.relay_list().request_id(), message.relay_list());
        else if (message.has_client_list())
            state_.dispatch(message.client_list().request_id(), message.client_list());
        else if (message.has_user_list())
            state_.dispatch(message.user_list().request_id(), message.user_list());
        else if (message.has_user_result())
        {
            // A successful add/modify/delete can move workspace state too: adding an
            // administrator grants it access entries and removing a user drops them by cascade,
            // both bumping the revisions of the affected workspaces. Same false-conflict window
            // as below. reset_otp/revoke_tokens do not touch workspaces, so the cache survives.
            const proto::router::UserResult& user_result = message.user_result();
            const std::string& command = user_result.command_name();
            const bool moves_workspaces = command == proto::router::kCommandUserAdd ||
                                          command == proto::router::kCommandUserModify ||
                                          command == proto::router::kCommandUserDelete;
            if (moves_workspaces && user_result.error_code() == proto::router::kErrorOk)
                state_.invalidateWorkspaces();
            state_.dispatch(user_result.request_id(), user_result);
        }
        else if (message.has_host_result())
        {
            // Same reasoning as for the workspaces below: a successful host change makes the
            // cached host lists stale until the batched notification arrives.
            if (message.host_result().error_code() == proto::router::kErrorOk)
                state_.clearHostCache();
            state_.dispatch(message.host_result().request_id(), message.host_result());
        }
        else if (message.has_relay_result())
            state_.dispatch(message.relay_result().request_id(), message.relay_result());
        else if (message.has_client_result())
            state_.dispatch(message.client_result().request_id(), message.client_result());
        else if (message.has_workspace_result())
        {
            // A successful add/modify/delete makes the cached list (and the revisions in it)
            // stale right now; the notification that would trigger a reload is batched on the
            // router and arrives seconds later. Serving the stale cache meanwhile would base
            // the next edit on an outdated revision - a false conflict.
            if (message.workspace_result().error_code() == proto::router::kErrorOk)
                state_.invalidateWorkspaces();
            state_.dispatch(message.workspace_result().request_id(), message.workspace_result());
        }
        else if (message.has_peer_result())
            state_.dispatch(message.peer_result().request_id(), message.peer_result());
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
        {
            // Same reasoning as for the workspaces above: a successful host change makes the
            // cached host lists stale until the batched notification arrives.
            if (message.host_result().error_code() == proto::router::kErrorOk)
                state_.clearHostCache();
            state_.dispatch(message.host_result().request_id(), message.host_result());
        }
        else if (message.has_group_result())
        {
            // The result carries no workspace_id, so the whole group cache goes; group edits
            // are rare enough that the extra reload does not matter.
            if (message.group_result().error_code() == proto::router::kErrorOk)
                state_.clearGroupCache();
            state_.dispatch(message.group_result().request_id(), message.group_result());
        }
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
            state_.dispatch(message.connection_offer().request_id(), message.connection_offer());
        else if (message.has_host_status())
            state_.dispatch(message.host_status().request_id(), message.host_status());
        else if (message.has_host_list())
            state_.dispatch(message.host_list().request_id(), message.host_list());
        else if (message.has_host_search_result())
            state_.dispatch(message.host_search_result().request_id(), message.host_search_result());
        else if (message.has_temp_host_list())
            state_.dispatch(message.temp_host_list().request_id(), message.temp_host_list());
        else if (message.has_workspace_list())
            state_.dispatch(message.workspace_list().request_id(), message.workspace_list());
        else if (message.has_group_list())
            state_.dispatch(message.group_list().request_id(), message.group_list());
        else if (message.has_change_password_result())
            state_.dispatch(message.change_password_result().request_id(), message.change_password_result());
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
void Router::setStatus(Status status)
{
    if (status_ == status)
        return;
    status_ = status;

    // Anything but ONLINE means the session cannot answer: the lists we hold are no longer known
    // to be current, and the replies we still wait for will never arrive.
    if (status_ != Status::ONLINE)
    {
        state_.clearCaches();
        state_.clearPending();
    }

    emit sig_statusChanged(config_.routerId(), status_);
}

//--------------------------------------------------------------------------------------------------
void Router::connectWorker()
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onConnect, Qt::QueuedConnection,
                              config_.routerId());
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectWorker()
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onDisconnect, Qt::QueuedConnection,
                              config_.routerId());
}

//--------------------------------------------------------------------------------------------------
void Router::clearSessionState()
{
    state_.clearSession();
    version_ = QVersionNumber();
}

//--------------------------------------------------------------------------------------------------
void Router::emitSend(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onSendMessage, Qt::QueuedConnection,
                              config_.routerId(), channel_id, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::readUserKeys(const proto::router::UserKeys& user_keys)
{
    LOG(INFO) << "User keys received (user_id:" << user_keys.user_id() << ")";

    const RouterState::KeysResult result =
        state_.applyUserKeys(user_keys, SecureString(config_.password()));

    if (result == RouterState::KeysResult::PASSWORD_CHANGE_REQUIRED)
    {
        LOG(WARNING) << "User has no wrap key/salt; prompting password change";
        emit sig_passwordChangeRequired(config_.routerId());
        return;
    }

    if (result == RouterState::KeysResult::DECRYPT_FAILED)
        return;

    const QString router_guid = QString::fromStdString(user_keys.router_guid());
    if (!router_guid.isEmpty() && router_guid != config_.guid())
    {
        config_.setGuid(router_guid);
        if (!Database::instance().modifyRouter(config_))
            LOG(WARNING) << "Failed to persist GUID for router" << config_.routerId();
    }

    setStatus(Status::ONLINE);
}

//--------------------------------------------------------------------------------------------------
void Router::readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge)
{
    // The router re-opens the two-factor stage of a session that was already up (after our own
    // password change, which revokes every device token). From that moment it drops everything we
    // send until the stage completes, so the session must go back to CONNECTING: the replies we
    // wait for will never arrive, the cached lists are no longer known to be current, and the UI
    // must stop issuing requests into a window where they are silently discarded. UserKeys puts
    // the session back to ONLINE.
    if (status_ == Status::ONLINE)
        setStatus(Status::CONNECTING);

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

    if (notification.temp_hosts_dirty())
        emit sig_tempHostsChanged(router_id);
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
