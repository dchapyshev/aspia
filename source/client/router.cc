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

#include "base/core_application.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/serialization.h"
#include "build/build_config.h"
#include "client/database.h"
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

//--------------------------------------------------------------------------------------------------
// One host record, as both the list and the search reply carry it.
RouterHost parseHost(const proto::router::Host& src)
{
    RouterHost dst;
    dst.host_id       = src.host_id();
    dst.workspace_id  = src.workspace_id();
    dst.group_id      = src.group_id();
    dst.display_name  = QString::fromStdString(src.display_name());
    dst.computer_name = QString::fromStdString(src.computer_name());
    dst.cpu_arch      = QString::fromStdString(src.cpu_arch());
    dst.version       = QString::fromStdString(src.version());
    dst.os_name       = QString::fromStdString(src.os_name());
    dst.address       = QString::fromStdString(src.address());
    dst.comment       = QString::fromStdString(src.comment());
    dst.last_connect  = src.last_connect();
    dst.last_modify   = src.last_modify();
    dst.online        = src.online();
    dst.revision      = src.revision();

    return dst;
}

// The serialize* functions fill an outgoing record and check the protocol bounds. An oversized
// request is not refused by the router, it tears the session down.

//--------------------------------------------------------------------------------------------------
std::string_view serializeWorkspace(const RouterWorkspace& workspace, proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    // Trimmed here because that is the value the router stores and measures.
    out->set_name(workspace.name.trimmed().toStdString());
    out->set_comment(workspace.comment.toStdString());
    out->set_revision(workspace.revision);

    for (qint64 user_id : std::as_const(workspace.user_ids))
        out->add_user_id(user_id);

    // The name is mandatory. Sizes are of the bytes that go out, not of the text the user typed.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in workspace" << workspace.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view serializeGroup(const RouterGroup& group, proto::router::Group* out)
{
    CHECK(out);

    if (group.entry_id > 0)
        out->set_entry_id(group.entry_id);
    out->set_parent_id(group.parent_id);
    out->set_name(group.name.trimmed().toStdString());
    out->set_comment(group.comment.toStdString());
    out->set_revision(group.revision);

    // The name is mandatory.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in group" << group.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
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
void Router::listUsers(qint64 offset, qint64 count,
                       RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_offset(offset);
    request->set_count(count);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::findUser(qint64 entry_id, RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_entry_id(entry_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::findUser(const QString& name, RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_name(name.toStdString());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void Router::listUserTokens(qint64 user_id,
                            RouterCallback<proto::router::UserTokenList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_token_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_user_id(user_id);
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
                              RouterCallback<proto::router::UserTokenResult> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_token_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_command_name(proto::router::kCommandUserTokenRevoke);
    request->set_user_id(user_id);
    for (qint64 token_id : token_ids)
        request->add_token_id(token_id);
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
    serialized.set_host_id(host.host_id);
    serialized.set_workspace_id(host.workspace_id);
    serialized.set_group_id(host.group_id);
    serialized.set_display_name(host.display_name.toStdString());
    serialized.set_comment(host.comment.toStdString());
    serialized.set_revision(host.revision);

    // Every field of a host is optional (an empty display name falls back to the computer name),
    // so only the sizes are checked. They count the bytes that go out, not the characters typed.
    if (serialized.display_name().size() > proto::router::kMaxEntryNameLength ||
        serialized.comment().size() > proto::router::kMaxCommentLength)
    {
        // Nothing is sent, so no reply would ever come and the caller would wait forever.
        LOG(ERROR) << "Oversized field in host" << host.host_id;

        proto::router::HostResult result;
        result.set_error_code(std::string(proto::router::kErrorInvalidData));
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
    const std::string_view error_code = serializeWorkspace(workspace, &serialized);
    if (error_code != proto::router::kErrorOk)
    {
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(error_code));
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
    const std::string_view error_code = serializeWorkspace(workspace, &serialized);
    if (error_code != proto::router::kErrorOk)
    {
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(error_code));
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
    const std::string_view error_code = serializeGroup(group, &serialized);
    if (error_code != proto::router::kErrorOk)
    {
        proto::router::GroupResult result;
        result.set_error_code(std::string(error_code));
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
    const std::string_view error_code = serializeGroup(group, &serialized);
    if (error_code != proto::router::kErrorOk)
    {
        proto::router::GroupResult result;
        result.set_error_code(std::string(error_code));
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
        [](const proto::router::HostSearchResult& raw)
    {
        RouterHostList matches;
        matches.error_code = QString::fromStdString(raw.error_code());
        // The pagination takes a non-negative count as its contract, so a negative one stops here.
        matches.total_count = qMax<qint64>(0, raw.total_count());
        matches.hosts.reserve(raw.host_size());

        for (int i = 0; i < raw.host_size(); ++i)
            matches.hosts.append(parseHost(raw.host(i)));

        return matches;
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
        RouterTempHostList temp_hosts;
        temp_hosts.error_code = QString::fromStdString(raw.error_code());
        temp_hosts.hosts.reserve(raw.host_size());

        for (int i = 0; i < raw.host_size(); ++i)
        {
            const proto::router::TempHost& src = raw.host(i);

            RouterTempHost& dst = temp_hosts.hosts.emplaceBack();
            dst.temp_id       = src.temp_id();
            dst.computer_name = QString::fromStdString(src.computer_name());
            dst.version       = QString::fromStdString(src.version());
            dst.os_name       = QString::fromStdString(src.os_name());
            dst.address       = QString::fromStdString(src.address());
        }

        return temp_hosts;
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
void Router::requestConnection(HostId host_id, RouterCallback<proto::router::ConnectionOffer> callback)
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
    RouterUser new_user = RouterUser::create(user_name_, new_password);

    proto::router::ClientToRouter message;
    auto* request = message.mutable_change_password_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_salt(new_user.salt.toStdString());
    request->set_verifier(new_user.verifier.toStdString());

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
    // when UserInfo arrives.
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
        else if (message.has_user_info())
            readUserInfo(message.user_info());
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
    else if (message.has_user_token_list())
    {
        rpc_.dispatch(message.user_token_list().request_id(), message.user_token_list());
    }
    else if (message.has_user_token_result())
    {
        const proto::router::UserTokenResult& result = message.user_token_result();
        rpc_.dispatch(result.request_id(), result);
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
    RouterWorkspaceList result;
    result.error_code = QString::fromStdString(list.error_code());
    result.workspaces.reserve(list.workspace_size());

    for (int i = 0; i < list.workspace_size(); ++i)
    {
        const proto::router::Workspace& src = list.workspace(i);

        RouterWorkspace& dst = result.workspaces.emplaceBack();
        dst.entry_id = src.entry_id();
        dst.name     = QString::fromStdString(src.name());
        dst.comment  = QString::fromStdString(src.comment());
        dst.revision = src.revision();
        dst.user_ids.reserve(src.user_id_size());

        for (int j = 0; j < src.user_id_size(); ++j)
            dst.user_ids.append(src.user_id(j));
    }

    // Only the complete list is the authoritative answer about what we can access.
    if (requested_workspace_id == 0 && result.error_code == proto::router::kErrorOk)
        cache_.storeWorkspaces(result);

    return result;
}

//--------------------------------------------------------------------------------------------------
RouterHostList Router::applyHostList(const proto::router::HostList& list,
                                     const RouterCache::HostKey& key, bool cacheable)
{
    RouterHostList result;
    result.error_code   = QString::fromStdString(list.error_code());
    result.workspace_id = list.workspace_id();
    result.group_id     = list.group_id();
    // The pagination takes a non-negative count as its contract, so a negative one stops here.
    result.total_count  = qMax<qint64>(0, list.total_count());
    result.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
        result.hosts.append(parseHost(list.host(i)));

    if (cacheable)
        cache_.storeHosts(key, result);

    return result;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList Router::applyGroupList(const proto::router::GroupList& list)
{
    const qint64 workspace_id = list.workspace_id();

    RouterGroupList result;
    result.error_code   = QString::fromStdString(list.error_code());
    result.workspace_id = workspace_id;
    result.groups.reserve(list.group_size());

    for (int i = 0; i < list.group_size(); ++i)
    {
        const proto::router::Group& src = list.group(i);

        RouterGroup& dst = result.groups.emplaceBack();
        dst.entry_id     = src.entry_id();
        dst.workspace_id = workspace_id;
        dst.parent_id    = src.parent_id();
        dst.name         = QString::fromStdString(src.name());
        dst.comment      = QString::fromStdString(src.comment());
        dst.revision     = src.revision();
    }

    cache_.storeGroups(result);
    return result;
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
    user_id_ = 0;
    user_name_.clear();
    rpc_.clearPending();
    version_ = QVersionNumber();
}

//--------------------------------------------------------------------------------------------------
void Router::send(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    emit sig_sendMessage(config_.routerId(), channel_id, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Router::readUserInfo(const proto::router::UserInfo& user_info)
{
    LOG(INFO) << "User info received (user_id:" << user_info.user_id() << ")";

    user_id_ = user_info.user_id();
    user_name_ = QString::fromStdString(user_info.name());

    const QString router_guid = QString::fromStdString(user_info.router_guid());
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
    // session goes back to CONNECTING; UserInfo puts it back to ONLINE.
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
    // connection instead. Persist the token. Final success is marked separately by UserInfo.
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
