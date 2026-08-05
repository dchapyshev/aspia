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

#ifndef CLIENT_ROUTER_H
#define CLIENT_ROUTER_H

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QVersionNumber>

#include <google/protobuf/message_lite.h>

#include "base/crypto/secure_byte_array.h"
#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "client/config.h"
#include "client/router_state.h"
#include "client/router_types.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

class RouterWorker;

class Router final : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        OFFLINE,
        CONNECTING,
        ONLINE
    };
    Q_ENUM(Status)

    enum class CachePolicy
    {
        USE_CACHE, // Return the cached result when available; otherwise fetch and cache it.
        RELOAD     // Always fetch from the server and refresh the cache.
    };
    Q_ENUM(CachePolicy)

    // The plain (decrypted) records the session works with. Defined in router_types.h so that
    // RouterState can produce them without depending on this class; the names below are what the
    // call sites use.
    using Workspace     = RouterWorkspace;
    using WorkspaceList = RouterWorkspaceList;
    using Host          = RouterHost;
    using HostList      = RouterHostList;
    using TempHost      = RouterTempHost;
    using TempHostList  = RouterTempHostList;
    using Group         = RouterGroup;
    using GroupList     = RouterGroupList;

    explicit Router(const RouterConfig& config, QObject* parent = nullptr);
    ~Router() final;

    // Lookup an existing instance by router id. Returns nullptr if no Router with that router_id
    // exists in the current thread.
    static Router* instance(qint64 router_id);

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    // Forwarded from the authenticator when the server demands a TOTP code. Callers respond by
    // calling submitTwoFactorCode(); cancelling the dialog should trigger disconnectFromRouter()
    // instead. Any successful TOTP submission causes the router to issue a fresh bearer token
    // which Router persists locally - no client-side opt-in required.
    void submitTwoFactorCode(const QString& totp_code);

    Status status() const { return status_; }
    QVersionNumber version() const { return version_; }
    qint64 routerId() const { return config_.routerId(); }
    const RouterConfig& config() const { return config_; }

    //----------------------------------------------------------------------------------------------
    // Admin: list queries.
    //----------------------------------------------------------------------------------------------

    template<typename HandlerT>
    void listRelays(QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void listClients(QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void listUsers(QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Admin: user operations.
    //----------------------------------------------------------------------------------------------

    template<typename HandlerT>
    void addUser(const proto::router::User& user, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void modifyUser(const proto::router::User& user, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void deleteUser(qint64 entry_id, QObject* receiver, HandlerT handler);

    // Clear |user_id|'s TOTP secret so the next login triggers fresh enrollment. Also
    // revokes every device token of the user (re-enrollment implies a new device key pair).
    template<typename HandlerT>
    void resetUserOtp(qint64 user_id, QObject* receiver, HandlerT handler);

    // Revoke the listed device tokens of |user_id|. An empty list means "revoke every token of this
    // user" and is handled atomically server-side.
    template<typename HandlerT>
    void revokeUserTokens(qint64 user_id, const QList<qint64>& token_ids,
                          QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Admin: host operations.
    //----------------------------------------------------------------------------------------------

    // Pass kAllHostsId to target all hosts.
    template<typename HandlerT>
    void disconnectHost(HostId host_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void removeHost(HostId host_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void approveHost(HostId host_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void checkHostUpdates(HostId host_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void editHost(const Router::Host& host, QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Admin: relay/client/peer disconnect.
    //----------------------------------------------------------------------------------------------

    // session_id == -1 means "all".
    template<typename HandlerT>
    void disconnectRelay(qint64 session_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void disconnectClient(qint64 session_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void disconnectPeer(qint64 relay_id, qint64 peer_id, QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Admin: workspace operations.
    //----------------------------------------------------------------------------------------------

    template<typename HandlerT>
    void addWorkspace(const Router::Workspace& workspace, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void modifyWorkspace(const Router::Workspace& workspace, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void deleteWorkspace(qint64 entry_id, QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Admin: host-group operations.
    //----------------------------------------------------------------------------------------------

    template<typename HandlerT>
    void addGroup(qint64 workspace_id, const Router::Group& group, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void modifyGroup(qint64 workspace_id, const Router::Group& group, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void deleteGroup(qint64 workspace_id, qint64 entry_id, QObject* receiver, HandlerT handler);

    //----------------------------------------------------------------------------------------------
    // Client.
    //----------------------------------------------------------------------------------------------

    // Workspace list (response is the decoded plain struct). workspace_id == 0 returns all visible
    // workspaces; > 0 narrows to a single entry. With |policy| == USE_CACHE the cached result of the
    // all-workspaces query is returned without a request when available.
    template<typename HandlerT>
    void listWorkspaces(CachePolicy policy, qint64 workspace_id, QObject* receiver, HandlerT handler);

    // Host-group list of a workspace (response is the decoded plain struct with comments decrypted
    // via the cached workspace cryptor). With |policy| == USE_CACHE a cached result is returned
    // without a request when available.
    template<typename HandlerT>
    void listGroups(CachePolicy policy, qint64 workspace_id, QObject* receiver, HandlerT handler);

    // Host list. Caller supplies a pre-filled request (mode, filters, etc). Filtered queries are
    // cached; with |policy| == USE_CACHE a cached result is returned without a request when available.
    template<typename HandlerT>
    void listHosts(CachePolicy policy, proto::router::HostListRequest request, QObject* receiver,
                   HandlerT handler);

    // Substring search over hosts by display name and host_id across every workspace accessible
    // to the user. The page is mandatory, exactly as for listHosts; total_count of the answer is
    // the number of matches in the whole scope. Results are not cached.
    template<typename HandlerT>
    void searchHosts(const QString& query, qint64 offset, qint64 count, QObject* receiver,
                     HandlerT handler);

    // List the temporary (unapproved) hosts currently online. Available to every session type;
    // |address| in each entry is populated only for admin sessions.
    template<typename HandlerT>
    void listTempHosts(QObject* receiver, HandlerT handler);

    // Ask the router whether a host is currently online.
    template<typename HandlerT>
    void checkHostStatus(HostId host_id, QObject* receiver, HandlerT handler);

    // Ask the router for a relay connection offer to the given host.
    template<typename HandlerT>
    void requestConnection(HostId host_id, QObject* receiver, HandlerT handler);

    // Rotate the password of the authenticated user. On success the router re-runs the 2FA
    // stage (see the server), so the client will be prompted for a code again afterwards.
    template<typename HandlerT>
    void changePassword(const SecureString& new_password, QObject* receiver, HandlerT handler);

signals:
    void sig_statusChanged(qint64 router_id, Router::Status status);
    void sig_errorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void sig_passwordChangeRequired(qint64 router_id);
    void sig_twoFactorCodeRequired(qint64 router_id);
    void sig_twoFactorEnrollment(qint64 router_id, const QString& otpauth_uri);

    // Push notifications: server signals that a particular list has changed and subscribers
    // should refetch via the corresponding list* RPC. Fired at most once per ~5 seconds per
    // resource. Regular client sessions only receive sig_tempHostsChanged / sig_hostsChanged /
    // sig_workspacesChanged / sig_groupsChanged.
    void sig_tempHostsChanged(qint64 router_id);
    void sig_hostsChanged(qint64 router_id);
    void sig_relaysChanged(qint64 router_id);
    void sig_clientsChanged(qint64 router_id);
    void sig_usersChanged(qint64 router_id);
    void sig_workspacesChanged(qint64 router_id);
    void sig_groupsChanged(qint64 router_id);

private slots:
    void onTcpAuthenticated(qint64 router_id, const QVersionNumber& peer_version);
    void onTcpErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(qint64 router_id, quint8 channel_id, const QByteArray& bytes);

private:
    void setStatus(Status status);
    void connectWorker();
    void disconnectWorker();
    void clearSessionState();
    void emitSend(quint8 channel_id, const google::protobuf::MessageLite& message);
    void readUserKeys(const proto::router::UserKeys& user_keys);
    void readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge);
    void readTwoFactorResult(const proto::router::TwoFactorResult& result);
    void persistChangedPassword(const SecureString& new_password);
    void emitNotificationSignals(const proto::router::Notification& notification);

    RouterConfig config_;
    QPointer<RouterWorker> router_worker_;
    QVersionNumber version_;
    Status status_ = Status::OFFLINE;

    // Identity, group keys, pending replies and the decoded list caches of this session.
    RouterState state_;

    Q_DISABLE_COPY_MOVE(Router)
};

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listRelays(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::RelayList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listClients(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::ClientList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listUsers(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::UserList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::addUser(const proto::router::User& user, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandUserAdd);
    request->mutable_user()->CopyFrom(user);

    // An administrator has access to every workspace, so our workspace keys are sealed to the key
    // pair of the new user and become its access entries on the router.
    if (user.sessions() & proto::router::SESSION_TYPE_ADMIN)
        state_.resealGroupKeys(QByteArray::fromStdString(user.public_key()), request->mutable_user());

    state_.rpc().registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::modifyUser(const proto::router::User& user, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandUserModify);
    request->mutable_user()->CopyFrom(user);
    state_.resealGroupKeys(QByteArray::fromStdString(user.public_key()), request->mutable_user());
    state_.rpc().registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::deleteUser(qint64 entry_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandUserDelete);
    request->mutable_user()->set_entry_id(entry_id);
    state_.rpc().registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::resetUserOtp(qint64 user_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandUserResetOtp);
    request->mutable_user()->set_entry_id(user_id);
    state_.rpc().registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::revokeUserTokens(qint64 user_id, const QList<qint64>& token_ids,
                              QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandUserRevokeTokens);
    auto* user = request->mutable_user();
    user->set_entry_id(user_id);
    for (qint64 token_id : token_ids)
        user->add_token()->set_token_id(token_id);
    state_.rpc().registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::disconnectHost(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandHostDisconnect);
    request->mutable_host()->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::removeHost(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandHostRemove);
    request->mutable_host()->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::approveHost(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandHostApprove);
    request->mutable_host()->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::checkHostUpdates(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandHostUpdate);
    request->mutable_host()->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::editHost(const Router::Host& host, QObject* receiver, HandlerT handler)
{
    proto::router::Host serialized;
    const std::string_view build_error = state_.buildHost(host, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply will come. Without this the caller waits forever.
        proto::router::HostResult result;
        result.set_error_code(std::string(build_error));
        RouterRpc::invokeHandler(receiver, handler, result);
        return;
    }
    proto::router::ManagerToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandHostModify);
    request->mutable_host()->Swap(&serialized);
    state_.rpc().registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::disconnectRelay(qint64 session_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandRelayDisconnect);
    request->set_entry_id(session_id);
    state_.rpc().registerPending<proto::router::RelayResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::disconnectClient(qint64 session_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandClientDisconnect);
    request->set_entry_id(session_id);
    state_.rpc().registerPending<proto::router::ClientResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::disconnectPeer(qint64 relay_id, qint64 peer_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_peer_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandPeerDisconnect);
    request->set_relay_id(relay_id);
    request->set_peer_id(peer_id);
    state_.rpc().registerPending<proto::router::PeerResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::addWorkspace(const Router::Workspace& workspace, QObject* receiver, HandlerT handler)
{
    proto::router::Workspace ws;
    const std::string_view build_error = state_.buildWorkspace(workspace, &ws);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply will come. Without this the caller waits forever.
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(build_error));
        RouterRpc::invokeHandler(receiver, handler, result);
        return;
    }
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceAdd);
    request->mutable_workspace()->Swap(&ws);
    state_.rpc().registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::modifyWorkspace(const Router::Workspace& workspace, QObject* receiver, HandlerT handler)
{
    proto::router::Workspace ws;
    const std::string_view build_error = state_.buildWorkspace(workspace, &ws);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply will come. Without this the caller waits forever.
        proto::router::WorkspaceResult result;
        result.set_error_code(std::string(build_error));
        RouterRpc::invokeHandler(receiver, handler, result);
        return;
    }
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceModify);
    request->mutable_workspace()->Swap(&ws);
    state_.rpc().registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::deleteWorkspace(qint64 entry_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceDelete);
    request->mutable_workspace()->set_entry_id(entry_id);
    state_.rpc().registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::addGroup(qint64 workspace_id, const Router::Group& group, QObject* receiver, HandlerT handler)
{
    proto::router::Group serialized;
    const std::string_view build_error = state_.buildGroup(workspace_id, group, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply will come. Without this the caller waits forever.
        proto::router::GroupResult result;
        result.set_error_code(std::string(build_error));
        RouterRpc::invokeHandler(receiver, handler, result);
        return;
    }
    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandGroupAdd);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->Swap(&serialized);
    state_.rpc().registerPending<proto::router::GroupResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::modifyGroup(qint64 workspace_id, const Router::Group& group, QObject* receiver, HandlerT handler)
{
    proto::router::Group serialized;
    const std::string_view build_error = state_.buildGroup(workspace_id, group, &serialized);
    if (build_error != proto::router::kErrorOk)
    {
        // Nothing is sent, so no reply will come. Without this the caller waits forever.
        proto::router::GroupResult result;
        result.set_error_code(std::string(build_error));
        RouterRpc::invokeHandler(receiver, handler, result);
        return;
    }
    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandGroupModify);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->Swap(&serialized);
    state_.rpc().registerPending<proto::router::GroupResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::deleteGroup(qint64 workspace_id, qint64 entry_id, QObject* receiver, HandlerT handler)
{
    proto::router::ManagerToRouter message;
    auto* request = message.mutable_group_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_command_name(proto::router::kCommandGroupDelete);
    request->set_workspace_id(workspace_id);
    request->mutable_group()->set_entry_id(entry_id);
    state_.rpc().registerPending<proto::router::GroupResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_MANAGER, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listWorkspaces(CachePolicy policy, qint64 workspace_id, QObject* receiver, HandlerT handler)
{
    if (policy == CachePolicy::USE_CACHE && workspace_id == 0 && state_.workspacesLoaded())
    {
        RouterRpc::invokeHandler(receiver, handler, state_.cachedWorkspaceList());
        return;
    }

    proto::router::ClientToRouter message;
    auto* request = message.mutable_workspace_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_workspace_id(workspace_id);
    state_.rpc().registerPending<proto::router::WorkspaceList>(request, receiver, std::move(handler),
        [this, workspace_id](const proto::router::WorkspaceList& raw)
    {
        return state_.applyWorkspaceList(raw, workspace_id);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listGroups(CachePolicy policy, qint64 workspace_id, QObject* receiver, HandlerT handler)
{
    if (policy == CachePolicy::USE_CACHE)
    {
        const Router::GroupList* cached = state_.cachedGroupList(workspace_id);
        if (cached)
        {
            RouterRpc::invokeHandler(receiver, handler, *cached);
            return;
        }
    }

    proto::router::ClientToRouter message;
    auto* request = message.mutable_group_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_workspace_id(workspace_id);
    state_.rpc().registerPending<proto::router::GroupList>(request, receiver, std::move(handler),
        [this](const proto::router::GroupList& raw)
    {
        return state_.applyGroupList(raw);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listHosts(CachePolicy policy, proto::router::HostListRequest request, QObject* receiver,
                       HandlerT handler)
{
    // Only filtered (per workspace/group) queries are cached; the unfiltered admin query is not.
    // The requested page is part of the key: two pages of the same selection are different
    // answers, and the total count of the whole scope travels with them.
    const bool cacheable = request.mode() == proto::router::HostListRequest::MODE_FILTERED;
    const RouterState::HostCacheKey key{ request.workspace_id(), request.group_id(),
                                         request.offset(), request.count() };

    if (policy == CachePolicy::USE_CACHE && cacheable)
    {
        const Router::HostList* cached = state_.cachedHostList(key);
        if (cached)
        {
            RouterRpc::invokeHandler(receiver, handler, *cached);
            return;
        }
    }

    request.set_request_id(state_.rpc().nextRequestId());
    proto::router::ClientToRouter message;
    message.mutable_host_list_request()->Swap(&request);
    state_.rpc().registerPending<proto::router::HostList>(
        &message.host_list_request(), receiver, std::move(handler),
        [this, cacheable, key](const proto::router::HostList& raw)
    {
        return state_.applyHostList(raw, key, cacheable);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::searchHosts(const QString& query, qint64 offset, qint64 count, QObject* receiver,
                         HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_host_search_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_query(query.toStdString());
    request->set_offset(offset);
    request->set_count(count);
    state_.rpc().registerPending<proto::router::HostSearchResult>(request, receiver, std::move(handler),
        [this](const proto::router::HostSearchResult& raw)
    {
        return state_.decodeHostSearchResult(raw);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::listTempHosts(QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_temp_host_list_request();
    request->set_request_id(state_.rpc().nextRequestId());
    state_.rpc().registerPending<proto::router::TempHostList>(request, receiver, std::move(handler),
        [this](const proto::router::TempHostList& raw)
    {
        return state_.decodeTempHostList(raw);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::checkHostStatus(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_check_host_status();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::HostStatus>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::requestConnection(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_connection_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_host_id(host_id);
    state_.rpc().registerPending<proto::router::ConnectionOffer>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::changePassword(const SecureString& new_password, QObject* receiver, HandlerT handler)
{
    RouterUser new_user = RouterUser::create(state_.userName(), new_password);

    proto::router::ClientToRouter message;
    auto* request = message.mutable_change_password_request();
    request->set_request_id(state_.rpc().nextRequestId());
    request->set_salt(new_user.salt.toStdString());
    request->set_verifier(new_user.verifier.toStdString());
    request->set_public_key(new_user.public_key.toStdString());
    request->set_wrap_private_key(new_user.wrap_private_key.toStdString());
    request->set_wrap_salt(new_user.wrap_salt.toStdString());

    state_.resealGroupKeys(new_user.public_key, request);

    state_.rpc().registerPending<proto::router::ChangePasswordResult>(request, receiver,
        [this, new_password, handler = std::move(handler)](const proto::router::ChangePasswordResult& result)
    {
        if (result.error_code() == proto::router::kErrorOk)
            persistChangedPassword(new_password);
        handler(result);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

#endif // CLIENT_ROUTER_H
