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

#include "client/router_session.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/serialization.h"
#include "build/build_config.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "client/workers/router_worker.h"
#include "proto/router_constants.h"

namespace {

//--------------------------------------------------------------------------------------------------
struct Registrator
{
    Registrator()
    {
        qRegisterMetaType<RouterWorkspace>();
        qRegisterMetaType<RouterWorkspaceList>();
        qRegisterMetaType<RouterHost>();
        qRegisterMetaType<RouterHostList>();
        qRegisterMetaType<RouterTempHost>();
        qRegisterMetaType<RouterTempHostList>();
        qRegisterMetaType<RouterGroup>();
        qRegisterMetaType<RouterGroupList>();
    }
};

static volatile Registrator registrator;

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
RouterSession::RouterSession(SharedPointer<RouterConfig> config, qint64 user_id,
                             const QVersionNumber& peer_version, QObject* parent)
    : QObject(parent),
      config_(std::move(config)),
      version_(peer_version),
      user_id_(user_id)
{
    LOG(INFO) << "Ctor";

    router_worker_ = GuiApplication::findWorker<RouterWorker>();
    if (!router_worker_)
        LOG(ERROR) << "Router worker not found";
}

//--------------------------------------------------------------------------------------------------
RouterSession::~RouterSession()
{
    LOG(INFO) << "Dtor";
    rpc_.clearPending();
}

//--------------------------------------------------------------------------------------------------
bool RouterSession::storeCredentials(const QString& user_name, const SecureString& password)
{
    LOG(INFO) << "Credentials changed for router" << config_->routerId();

    // The rotation revoked every device token of the account on the router, this one included.
    // Dropping it now spares the next login a doomed token round.
    config_->clearDeviceToken();

    config_->setUsername(user_name);
    config_->setPassword(password);
    if (!Database::instance().modifyRouter(*config_))
    {
        LOG(WARNING) << "Failed to persist new credentials for router" << config_->routerId();
        RouterController::instance().addEvent(config_->routerId(), RouterEvent::Severity::WARNING,
            tr("The router accepted the new password, but the record was not updated."));
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterSession::listRelays(RouterCallback<proto::router::RelayList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::listClients(RouterCallback<proto::router::ClientList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_list_request();
    request->set_request_id(rpc_.nextRequestId());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::listUsers(qint64 offset, qint64 count,
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
void RouterSession::findUser(qint64 entry_id, RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_entry_id(entry_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::findUser(const QString& name, RouterCallback<proto::router::UserList> callback)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_name(name.toStdString());
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::listUserTokens(qint64 user_id,
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
void RouterSession::addUser(const proto::router::User& user,
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
void RouterSession::modifyUser(const proto::router::User& user,
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
void RouterSession::deleteUser(qint64 entry_id, RouterCallback<proto::router::UserResult> callback)
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
void RouterSession::resetUserOtp(qint64 user_id, RouterCallback<proto::router::UserResult> callback)
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
void RouterSession::revokeUserTokens(qint64 user_id, const QList<qint64>& token_ids,
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
void RouterSession::disconnectRelay(qint64 session_id, RouterCallback<proto::router::RelayResult> callback)
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
void RouterSession::disconnectClient(qint64 session_id,
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
void RouterSession::disconnectPeer(qint64 relay_id, qint64 peer_id,
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
void RouterSession::disconnectHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
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
void RouterSession::removeHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
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
void RouterSession::approveHost(HostId host_id, RouterCallback<proto::router::HostResult> callback)
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
void RouterSession::checkHostUpdates(HostId host_id, RouterCallback<proto::router::HostResult> callback)
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
void RouterSession::editHost(const RouterHost& host, RouterCallback<proto::router::HostResult> callback)
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
void RouterSession::addWorkspace(const RouterWorkspace& workspace,
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
void RouterSession::modifyWorkspace(const RouterWorkspace& workspace,
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
void RouterSession::deleteWorkspace(qint64 entry_id,
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
void RouterSession::addGroup(qint64 workspace_id, const RouterGroup& group,
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
void RouterSession::modifyGroup(qint64 workspace_id, const RouterGroup& group,
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
void RouterSession::deleteGroup(qint64 workspace_id, qint64 entry_id,
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
void RouterSession::listWorkspaces(CachePolicy policy, qint64 workspace_id,
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
void RouterSession::listGroups(CachePolicy policy, qint64 workspace_id,
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
void RouterSession::listHosts(CachePolicy policy, proto::router::HostListRequest request,
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
void RouterSession::searchHosts(const QString& query, qint64 offset, qint64 count,
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
void RouterSession::listTempHosts(qint64 offset, qint64 count, RouterCallback<RouterTempHostList> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_temp_host_list_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_offset(offset);
    request->set_count(count);
    rpc_.registerPending<proto::router::TempHostList>(request, std::move(callback),
        [](const proto::router::TempHostList& raw)
    {
        RouterTempHostList temp_hosts;
        temp_hosts.error_code = QString::fromStdString(raw.error_code());
        temp_hosts.total_count = qMax<qint64>(0, raw.total_count());
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
void RouterSession::checkHostStatus(HostId host_id, RouterCallback<proto::router::HostStatus> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_check_host_status();
    request->set_request_id(rpc_.nextRequestId());
    request->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::requestConnection(HostId host_id, RouterCallback<proto::router::ConnectionOffer> callback)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_connection_request();
    request->set_request_id(rpc_.nextRequestId());
    request->set_host_id(host_id);
    rpc_.registerPending(request, std::move(callback));
    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::changePassword(const SecureString& new_password,
                            RouterCallback<proto::router::ChangePasswordResult> callback)
{
    RouterUser new_user = RouterUser::create(config_->username(), new_password);

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
            storeCredentials(config_->username(), new_password);
        callback(result);
    }));

    send(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
void RouterSession::onMessageReceived(const proto::router::RouterToAdmin& message)
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
        LOG(WARNING) << "Unhandled admin message";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterSession::onMessageReceived(const proto::router::RouterToManager& message)
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
        LOG(WARNING) << "Unhandled manager message";
    }
}

//--------------------------------------------------------------------------------------------------
void RouterSession::onMessageReceived(const proto::router::RouterToClient& message)
{
    if (message.has_notification())
        cache_.onNotification(message.notification());
    else if (message.has_connection_offer())
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
        LOG(WARNING) << "Unhandled client message";
}

//--------------------------------------------------------------------------------------------------
void RouterSession::send(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    if (!router_worker_)
        return;

    QMetaObject::invokeMethod(router_worker_, &RouterWorker::onSendMessage, Qt::QueuedConnection,
                              config_->routerId(), channel_id, serialize(message));
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList RouterSession::applyWorkspaceList(const proto::router::WorkspaceList& list,
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
RouterHostList RouterSession::applyHostList(const proto::router::HostList& list,
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
RouterGroupList RouterSession::applyGroupList(const proto::router::GroupList& list)
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
