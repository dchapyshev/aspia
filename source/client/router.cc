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

#include <set>

#include "base/core_application.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/serialization.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/router_codec.h"
#include "client/workers/router_worker.h"
#include "proto/router_constants.h"

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

    // The interface runs on GuiApplication, the headless tools on CoreApplication.
    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
        router_worker_ = CoreApplication::findWorker<RouterWorker>();

    if (!router_worker_)
    {
        LOG(ERROR) << "Router worker not found";
        return;
    }

    connect(router_worker_, &RouterWorker::sig_authenticated, this, &Router::onTcpAuthenticated,
            Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_errorOccurred, this, &Router::onTcpErrorOccurred,
            Qt::QueuedConnection);
    connect(router_worker_, &RouterWorker::sig_messageReceived, this, &Router::onTcpMessageReceived,
            Qt::QueuedConnection);
    connect(this, &Router::sig_sendMessage, router_worker_, &RouterWorker::onSendMessage,
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
    // The status moves first, so the callers answered by clearSessionState() below already see a
    // session that cannot serve them.
    disconnectWorker();
    setStatus(Status::OFFLINE);
    clearSessionState();
}

//--------------------------------------------------------------------------------------------------
void Router::updateConfig(const RouterConfig& config)
{
    const bool need_reconnect = !config_.hasSameParams(config);
    config_ = config;

    if (need_reconnect && status_ != Status::OFFLINE)
    {
        disconnectWorker();
        setStatus(Status::CONNECTING);
        clearSessionState();
        connectWorker();
    }
}

//--------------------------------------------------------------------------------------------------
void Router::submitTwoFactorCode(const QString& totp_code)
{
    proto::router::ClientToRouter message;
    proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
    response->set_totp_code(totp_code.toStdString());
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listRelays(RouterCallback<proto::router::RelayList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listClients(RouterCallback<proto::router::ClientList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listUsers(RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::addUser(const proto::router::User& user,
                     RouterCallback<proto::router::UserResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserAdd);
    request->mutable_user()->CopyFrom(user);

    // An administrator has access to every workspace, so our workspace keys are sealed to the key
    // pair of the new user and become its access entries on the router.
    if (user.sessions() & proto::router::SESSION_TYPE_ADMIN)
        keys_.resealGroupKeys(QByteArray::fromStdString(user.public_key()), request->mutable_user());

    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::modifyUser(const proto::router::User& user,
                        RouterCallback<proto::router::UserResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserModify);
    request->mutable_user()->CopyFrom(user);
    keys_.resealGroupKeys(QByteArray::fromStdString(user.public_key()), request->mutable_user());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::deleteUser(qint64 entry_id, RouterCallback<proto::router::UserResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserDelete);
    request->mutable_user()->set_entry_id(entry_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::resetUserOtp(qint64 user_id, RouterCallback<proto::router::UserResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserResetOtp);
    request->mutable_user()->set_entry_id(user_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::revokeUserTokens(qint64 user_id, const QList<qint64>& token_ids,
                              RouterCallback<proto::router::UserResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserRevokeTokens);
    auto* user = request->mutable_user();
    user->set_entry_id(user_id);
    for (qint64 token_id : token_ids)
        user->add_token()->set_token_id(token_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectRelay(qint64 session_id, RouterCallback<proto::router::RelayResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandRelayDisconnect);
    request->set_entry_id(session_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectClient(qint64 session_id,
                              RouterCallback<proto::router::ClientResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandClientDisconnect);
    request->set_entry_id(session_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectPeer(qint64 relay_id, qint64 peer_id,
                            RouterCallback<proto::router::PeerResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_peer_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandPeerDisconnect);
    request->set_relay_id(relay_id);
    request->set_peer_id(peer_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::disconnectHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandHostDisconnect);
    request->mutable_host()->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::removeHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandHostRemove);
    request->mutable_host()->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::approveHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandHostApprove);
    request->mutable_host()->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::checkHostUpdates(HostId host_id, RouterCallback<proto::router::HostResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandHostUpdate);
    request->mutable_host()->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::editHost(const RouterHost& host, RouterCallback<proto::router::HostResult> callback)
{
    proto::router::Host serialized;
    const std::string_view build_error = buildRouterHost(keys_, host, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply would ever come and the caller would wait forever.
        proto::router::HostResult result;
        result.set_error_code(std::string(build_error));
        callback(result);
        return;
    }

    proto::router::ManagerToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandHostModify);
    request->mutable_host()->Swap(&serialized);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
void Router::addWorkspace(const RouterWorkspace& workspace,
                          RouterCallback<proto::router::WorkspaceResult> callback)
{
    proto::router::Workspace serialized;
    const std::string_view build_error = buildRouterWorkspace(keys_, workspace, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(build_error));
        callback(result);
        return;
    }

    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceAdd);
    request->mutable_workspace()->Swap(&serialized);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::modifyWorkspace(const RouterWorkspace& workspace,
                             RouterCallback<proto::router::WorkspaceResult> callback)
{
    proto::router::Workspace serialized;
    const std::string_view build_error = buildRouterWorkspace(keys_, workspace, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(build_error));
        callback(result);
        return;
    }

    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceModify);
    request->mutable_workspace()->Swap(&serialized);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::deleteWorkspace(qint64 entry_id,
                             RouterCallback<proto::router::WorkspaceResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceDelete);
    request->mutable_workspace()->set_entry_id(entry_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::addGroup(qint64 workspace_id, const RouterGroup& group,
                      RouterCallback<proto::router::GroupResult> callback)
{
    proto::router::Group serialized;
    const std::string_view build_error = buildRouterGroup(keys_, workspace_id, group, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        proto::router::GroupResult result;
        result.set_error_code(std::string(build_error));
        callback(result);
        return;
    }

    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandGroupAdd);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->Swap(&serialized);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
void Router::modifyGroup(qint64 workspace_id, const RouterGroup& group,
                         RouterCallback<proto::router::GroupResult> callback)
{
    proto::router::Group serialized;
    const std::string_view build_error = buildRouterGroup(keys_, workspace_id, group, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        proto::router::GroupResult result;
        result.set_error_code(std::string(build_error));
        callback(result);
        return;
    }

    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandGroupModify);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->Swap(&serialized);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
void Router::deleteGroup(qint64 workspace_id, qint64 entry_id,
                         RouterCallback<proto::router::GroupResult> callback)
{
    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandGroupDelete);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->set_entry_id(entry_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listWorkspaces(CachePolicy policy, qint64 workspace_id,
                            RouterCallback<RouterWorkspaceList> callback)
{
    if (policy == CachePolicy::USE_CACHE && workspace_id == 0 && cache_.workspacesLoaded())
    {
        callback(cache_.workspaceList());
        return;
    }

    proto::router::ClientToRouter message;
    auto* request = message.mutable_workspace_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_workspace_id(workspace_id);
    rpc_.registerPending<proto::router::WorkspaceList>(request, std::move(callback),
        [this, workspace_id](const proto::router::WorkspaceList& raw)
    {
        return applyWorkspaceList(raw, workspace_id);
    });
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listGroups(CachePolicy policy, qint64 workspace_id,
                        RouterCallback<RouterGroupList> callback)
{
    if (policy == CachePolicy::USE_CACHE)
    {
        const RouterGroupList* cached = cache_.groupList(workspace_id);
        if (cached)
        {
            callback(*cached);
            return;
        }
    }

    proto::router::ClientToRouter message;
    auto* request = message.mutable_group_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_workspace_id(workspace_id);
    rpc_.registerPending<proto::router::GroupList>(request, std::move(callback),
        [this](const proto::router::GroupList& raw)
    {
        return applyGroupList(raw);
    });
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listHosts(CachePolicy policy, proto::router::HostListRequest request,
                       RouterCallback<RouterHostList> callback)
{
    // Only filtered (per workspace/group) queries are cached. The page is part of the key: two
    // pages of the same selection are different answers.
    const bool cacheable = request.mode() == proto::router::HostListRequest::MODE_FILTERED;
    const RouterCache::HostKey key{ request.workspace_id(), request.group_id(),
                                    request.offset(), request.count() };

    if (policy == CachePolicy::USE_CACHE && cacheable)
    {
        const RouterHostList* cached = cache_.hostList(key);
        if (cached)
        {
            callback(*cached);
            return;
        }
    }

    request.set_request_id(rpc_.nextRequestId());
    proto::router::ClientToRouter message;
    message.mutable_host_list_request()->Swap(&request);
    rpc_.registerPending<proto::router::HostList>(
        &message.host_list_request(), std::move(callback),
        [this, cacheable, key](const proto::router::HostList& raw)
    {
        return applyHostList(raw, key, cacheable);
    });
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::searchHosts(const QString& query, qint64 offset, qint64 count,
                         RouterCallback<RouterHostList> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_host_search_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_query(query.toStdString());
    request->set_offset(offset);
    request->set_count(count);
    rpc_.registerPending<proto::router::HostSearchResult>(request, std::move(callback),
        [this](const proto::router::HostSearchResult& raw)
    {
        return decodeRouterHostSearchResult(keys_, raw);
    });
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listTempHosts(RouterCallback<RouterTempHostList> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_temp_host_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending<proto::router::TempHostList>(request, std::move(callback),
        [](const proto::router::TempHostList& raw)
    {
        return decodeRouterTempHostList(raw);
    });
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::checkHostStatus(HostId host_id, RouterCallback<proto::router::HostStatus> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_check_host_status();
    request->set_request_id(rpc_.nextRequestId());
    request->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::requestConnection(HostId host_id,
                               RouterCallback<proto::router::ConnectionOffer> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_connection_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void Router::changePassword(const SecureString& new_password,
                            RouterCallback<proto::router::ChangePasswordResult> callback)
{
    RouterUser new_user = RouterUser::create(keys_.userName(), new_password);

    proto::router::ClientToRouter message;
    auto* request = message.mutable_change_password_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_salt(new_user.salt.toStdString());
    request->set_verifier(new_user.verifier.toStdString());
    request->set_public_key(new_user.public_key.toStdString());
    request->set_wrap_private_key(new_user.wrap_private_key.toStdString());
    request->set_wrap_salt(new_user.wrap_salt.toStdString());

    keys_.resealGroupKeys(new_user.public_key, request);

    // The accepted password becomes the stored one: from now on it is what opens the account.
    QObject* receiver = callback.receiver();
    rpc_.registerPending(request, RouterCallback<proto::router::ChangePasswordResult>(receiver,
        [this, new_password, callback = std::move(callback)](
            const proto::router::ChangePasswordResult& result)
    {
        if (result.error_code() == proto::router::kErrorOk)
            persistChangedPassword(new_password);
        callback(result);
    }));

    send(proto::router::CHANNEL_ID_CLIENT, message);
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

    if (status_ != Status::OFFLINE)
        setStatus(Status::CONNECTING);

    clearSessionState();

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

        if (!routeReply(message))
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

        if (!routeReply(message))
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

        // The session-level messages are read here, everything else answers a request.
        if (message.has_two_factor_challenge())
            readTwoFactorChallenge(message.two_factor_challenge());
        else if (message.has_two_factor_result())
            readTwoFactorResult(message.two_factor_result());
        else if (message.has_user_keys())
            readUserKeys(message.user_keys());
        else if (message.has_notification())
            emitNotificationSignals(message.notification());
        else if (!routeReply(message))
            LOG(WARNING) << "Unhandled client message";
    }
    else
    {
        LOG(WARNING) << "Unexpected message from channel" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
bool Router::routeReply(const proto::router::RouterToAdmin& message)
{
    if (message.has_relay_list())
    {
        rpc_.dispatch(message.relay_list().request_id(), message.relay_list());
    }
    else if (message.has_client_list())
    {
        rpc_.dispatch(message.client_list().request_id(), message.client_list());
    }
    else if (message.has_user_list())
    {
        rpc_.dispatch(message.user_list().request_id(), message.user_list());
    }
    else if (message.has_user_result())
    {
        const proto::router::UserResult& result = message.user_result();
        cache_.onResult(RouterCache::Result::USER, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);
        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_host_result())
    {
        const proto::router::HostResult& result = message.host_result();
        cache_.onResult(RouterCache::Result::HOST, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);
        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_relay_result())
    {
        rpc_.dispatch(message.relay_result().request_id(), message.relay_result());
    }
    else if (message.has_client_result())
    {
        rpc_.dispatch(message.client_result().request_id(), message.client_result());
    }
    else if (message.has_workspace_result())
    {
        const proto::router::WorkspaceResult& result = message.workspace_result();
        cache_.onResult(RouterCache::Result::WORKSPACE, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);
        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_peer_result())
    {
        rpc_.dispatch(message.peer_result().request_id(), message.peer_result());
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Router::routeReply(const proto::router::RouterToManager& message)
{
    if (message.has_host_result())
    {
        const proto::router::HostResult& result = message.host_result();
        cache_.onResult(RouterCache::Result::HOST, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_group_result())
    {
        const proto::router::GroupResult& result = message.group_result();
        cache_.onResult(RouterCache::Result::GROUP, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Router::routeReply(const proto::router::RouterToClient& message)
{
    // Nothing here invalidates a cache: these are read-only queries, and the one write among them
    // (the password change) re-seals the keys the user already holds without moving any revision.
    if (message.has_connection_offer())
        rpc_.dispatch(message.connection_offer().request_id(), message.connection_offer());
    else if (message.has_host_status())
        rpc_.dispatch(message.host_status().request_id(), message.host_status());
    else if (message.has_host_list())
        rpc_.dispatch(message.host_list().request_id(), message.host_list());
    else if (message.has_host_search_result())
        rpc_.dispatch(message.host_search_result().request_id(), message.host_search_result());
    else if (message.has_temp_host_list())
        rpc_.dispatch(message.temp_host_list().request_id(), message.temp_host_list());
    else if (message.has_workspace_list())
        rpc_.dispatch(message.workspace_list().request_id(), message.workspace_list());
    else if (message.has_group_list())
        rpc_.dispatch(message.group_list().request_id(), message.group_list());
    else if (message.has_change_password_result())
        rpc_.dispatch(message.change_password_result().request_id(), message.change_password_result());
    else
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList Router::applyWorkspaceList(const proto::router::WorkspaceList& list,
                                               qint64 requested_workspace_id)
{
    RouterWorkspaceList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.workspaces.reserve(list.workspace_size());

    // The complete list is the authoritative answer about what we still have access to.
    const bool full_list = requested_workspace_id == 0 &&
                           decoded.error_code == proto::router::kErrorOk;
    std::set<qint64> visible_ids;

    for (int i = 0; i < list.workspace_size(); ++i)
    {
        const proto::router::Workspace& src = list.workspace(i);

        RouterWorkspace& dst = decoded.workspaces.emplaceBack();
        dst.entry_id = src.entry_id();
        dst.name     = QString::fromStdString(src.name());
        dst.revision = src.revision();
        dst.access.reserve(src.access_size());

        visible_ids.insert(src.entry_id());

        QByteArray self_wrapped_gk;
        for (int j = 0; j < src.access_size(); ++j)
        {
            const proto::router::WorkspaceAccess& access = src.access(j);
            dst.access.emplaceBack().user_id = access.user_id();

            if (access.user_id() == keys_.userId())
                self_wrapped_gk = QByteArray::fromStdString(access.wrapped_gk());
        }

        if (self_wrapped_gk.isEmpty())
            continue;

        SecureByteArray gk = keys_.unwrapGroupKey(self_wrapped_gk);
        if (gk.isEmpty())
        {
            // See RouterKeys::apply: the missing cryptor makes reseal-dependent operations answer
            // "conflict" for this workspace with no way for a refetch to recover.
            LOG(ERROR) << "Failed to unwrap GK for workspace" << src.entry_id();
            continue;
        }

        DataCryptor cryptor(CipherType::AES256_GCM, gk);
        if (!src.comment().empty())
            dst.comment = decryptField(cryptor, src.comment());

        keys_.storeWorkspaceKey(src.entry_id(), std::move(cryptor));
    }

    if (full_list)
    {
        keys_.dropKeysExcept(visible_ids);
        cache_.storeWorkspaces(decoded);
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList Router::applyHostList(const proto::router::HostList& list,
                                     const RouterCache::HostKey& key, bool cacheable)
{
    RouterHostList decoded = decodeRouterHostList(keys_, list);

    if (cacheable)
        cache_.storeHosts(key, decoded);

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList Router::applyGroupList(const proto::router::GroupList& list)
{
    RouterGroupList decoded = decodeRouterGroupList(keys_, list);
    cache_.storeGroups(decoded);
    return decoded;
}

//--------------------------------------------------------------------------------------------------
void Router::setStatus(Status status)
{
    if (status_ == status)
        return;
    status_ = status;

    // The lists we hold are no longer known to be current. Dropped before the callers below are
    // answered: one of them can retry from inside its handler, and a retry that accepts a cached
    // answer must not be served the lists of the session that just died.
    if (status_ != Status::ONLINE)
        cache_.clear();

    // A reply only ever arrives inside the window its request was made in: below ONLINE the router
    // drops what we send, so the requests issued on the way up are as dead as the ones a lost
    // session leaves behind. Both are answered here.
    rpc_.clearPending();

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
    keys_.clear();
    rpc_.clearPending();
    version_ = QVersionNumber();
}

//--------------------------------------------------------------------------------------------------
void Router::send(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    emit sig_sendMessage(config_.routerId(), channel_id, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::readUserKeys(const proto::router::UserKeys& user_keys)
{
    LOG(INFO) << "User keys received (user_id:" << user_keys.user_id() << ")";

    const RouterKeys::Result result = keys_.apply(user_keys, SecureString(config_.password()));

    if (result == RouterKeys::Result::PASSWORD_CHANGE_REQUIRED)
    {
        LOG(WARNING) << "User has no wrap key/salt; prompting password change";
        emit sig_passwordChangeRequired(config_.routerId());
        return;
    }

    if (result == RouterKeys::Result::DECRYPT_FAILED)
    {
        // The password opened the account but not the private key stored with it, so every
        // workspace stays locked and even a password change cannot re-seal keys we cannot read.
        // Nothing recovers from this and a reconnect would repeat it forever.
        LOG(ERROR) << "Stored private key does not open with our password. Ending session";
        emit sig_errorOccurred(config_.routerId(), TcpChannel::ErrorCode::CRYPTO_ERROR);
        disconnectFromRouter();
        return;
    }

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
    // The two-factor stage can re-open on a session that was already up (our own password change
    // revokes every device token). Until it completes the router drops everything we send, so the
    // session goes back to CONNECTING; UserKeys puts it back to ONLINE.
    if (status_ == Status::ONLINE)
        setStatus(Status::CONNECTING);

    switch (challenge.mode())
    {
        case proto::router::TWO_FACTOR_MODE_ACTIVE:
        {
            // The second ask means the token we presented was rejected (revoked, password
            // changed, database wiped). Presenting it again would loop forever, so the stale
            // copy goes and the operator is asked for a code.
            if (challenge.token_rejected())
            {
                LOG(INFO) << "Router rejected device token - clearing local copy";
                config_.clearDeviceToken();
                if (!Database::instance().modifyRouter(config_))
                    LOG(WARNING) << "Failed to clear stale device token";

                emit sig_twoFactorCodeRequired(config_.routerId());
                return;
            }

            // A token from a previous successful TOTP skips the prompt entirely.
            const QByteArray token = config_.deviceToken();
            if (!token.isEmpty())
            {
                proto::router::ClientToRouter message;
                proto::router::TwoFactorResponse* response = message.mutable_two_factor_response();
                response->set_token(token.toStdString());
                send(proto::router::CHANNEL_ID_CLIENT, message);
                return;
            }

            LOG(INFO) << "Two-factor code required for router" << config_.routerId();
            emit sig_twoFactorCodeRequired(config_.routerId());
            return;
        }

        case proto::router::TWO_FACTOR_MODE_ENROLL:
        {
            const QString uri = QString::fromStdString(challenge.otpauth_uri());

            // Enrollment means a brand new TOTP secret, so a token of the previous account
            // life is dead.
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

    cache_.onNotification(notification);

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
