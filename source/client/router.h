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

#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/router_cache.h"
#include "client/router_keys.h"
#include "client/router_rpc.h"
#include "client/router_types.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_manager.h"

class RouterWorker;

// A session with a router: its requests, the replies routed back to their callers, the keys it
// holds and the lists it caches. The bytes travel through RouterWorker, which owns the socket.
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

    // The plain records the session works with. Declared in router_types.h so the cache and the
    // widgets can hold them without depending on this class.
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

    // Returns nullptr if no Router with that router_id exists in the current thread.
    static Router* instance(qint64 router_id);

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    // The answer to sig_twoFactorCodeRequired. A cancelled prompt calls disconnectFromRouter()
    // instead. An accepted code makes the router issue a device token, which is persisted here.
    void submitTwoFactorCode(const QString& totp_code);

    Status status() const { return status_; }
    QVersionNumber version() const { return version_; }
    qint64 routerId() const { return config_.routerId(); }
    const RouterConfig& config() const { return config_; }

    //----------------------------------------------------------------------------------------------
    // Admin: list queries.
    //----------------------------------------------------------------------------------------------

    void listRelays(RouterCallback<proto::router::RelayList> callback);
    void listClients(RouterCallback<proto::router::ClientList> callback);

    // One page of the user list. The reply carries the total, so the caller can page through it.
    void listUsers(qint64 offset, qint64 count, RouterCallback<proto::router::UserList> callback);

    // A single record, answered with an empty list when there is no such user.
    void findUser(qint64 entry_id, RouterCallback<proto::router::UserList> callback);
    void findUser(const QString& name, RouterCallback<proto::router::UserList> callback);

    // The active device tokens of one user.
    void listUserTokens(qint64 user_id, RouterCallback<proto::router::UserTokenList> callback);

    //----------------------------------------------------------------------------------------------
    // Admin: user operations.
    //----------------------------------------------------------------------------------------------

    void addUser(const proto::router::User& user,
                 RouterCallback<proto::router::UserResult> callback);
    void modifyUser(const proto::router::User& user,
                    RouterCallback<proto::router::UserResult> callback);
    void deleteUser(qint64 entry_id, RouterCallback<proto::router::UserResult> callback);

    // Clears the TOTP secret, so the next login enrolls anew. Revokes every device token too.
    void resetUserOtp(qint64 user_id, RouterCallback<proto::router::UserResult> callback);

    // An empty |token_ids| revokes every token of the user.
    void revokeUserTokens(qint64 user_id, const QList<qint64>& token_ids,
                          RouterCallback<proto::router::UserTokenResult> callback);

    //----------------------------------------------------------------------------------------------
    // Admin: relay/client/peer disconnect.
    //----------------------------------------------------------------------------------------------

    // session_id == -1 means "all".
    void disconnectRelay(qint64 session_id, RouterCallback<proto::router::RelayResult> callback);
    void disconnectClient(qint64 session_id, RouterCallback<proto::router::ClientResult> callback);
    void disconnectPeer(qint64 relay_id, qint64 peer_id,
                        RouterCallback<proto::router::PeerResult> callback);

    //----------------------------------------------------------------------------------------------
    // Host operations.
    //----------------------------------------------------------------------------------------------

    // Pass kAllHostsId to target all hosts.
    void disconnectHost(HostId host_id, RouterCallback<proto::router::HostResult> callback);
    void removeHost(HostId host_id, RouterCallback<proto::router::HostResult> callback);
    void approveHost(HostId host_id, RouterCallback<proto::router::HostResult> callback);
    void checkHostUpdates(HostId host_id, RouterCallback<proto::router::HostResult> callback);

    // A record that breaks the protocol bounds is answered with its error code without a request.
    // |workspace_id| is the workspace the host ends up in (0 releases it), and only an
    // administrator may change it.
    void editHost(const RouterHost& host, RouterCallback<proto::router::HostResult> callback);

    //----------------------------------------------------------------------------------------------
    // Workspace operations.
    //----------------------------------------------------------------------------------------------

    void addWorkspace(const RouterWorkspace& workspace,
                      RouterCallback<proto::router::WorkspaceResult> callback);
    void modifyWorkspace(const RouterWorkspace& workspace,
                         RouterCallback<proto::router::WorkspaceResult> callback);
    void deleteWorkspace(qint64 entry_id, RouterCallback<proto::router::WorkspaceResult> callback);

    //----------------------------------------------------------------------------------------------
    // Host-group operations.
    //----------------------------------------------------------------------------------------------

    void addGroup(qint64 workspace_id, const RouterGroup& group,
                  RouterCallback<proto::router::GroupResult> callback);
    void modifyGroup(qint64 workspace_id, const RouterGroup& group,
                     RouterCallback<proto::router::GroupResult> callback);
    void deleteGroup(qint64 workspace_id, qint64 entry_id,
                     RouterCallback<proto::router::GroupResult> callback);

    //----------------------------------------------------------------------------------------------
    // Client queries.
    //----------------------------------------------------------------------------------------------

    // |workspace_id| == 0 asks for every visible workspace, > 0 for a single entry. Only the
    // complete list is cached.
    void listWorkspaces(CachePolicy policy, qint64 workspace_id,
                        RouterCallback<RouterWorkspaceList> callback);

    void listGroups(CachePolicy policy, qint64 workspace_id,
                    RouterCallback<RouterGroupList> callback);

    // Caller supplies a pre-filled request (mode, filters, page). Only filtered queries are cached.
    void listHosts(CachePolicy policy, proto::router::HostListRequest request,
                   RouterCallback<RouterHostList> callback);

    // Substring search by display name and host id across every workspace of the user. The page is
    // mandatory as for listHosts; total_count counts the matches of the whole scope.
    void searchHosts(const QString& query, qint64 offset, qint64 count,
                     RouterCallback<RouterHostList> callback);

    // The temporary (unapproved) hosts currently online. |address| is filled for admins only.
    void listTempHosts(RouterCallback<RouterTempHostList> callback);

    void checkHostStatus(HostId host_id, RouterCallback<proto::router::HostStatus> callback);

    // Asks for a relay connection offer to the given host.
    void requestConnection(HostId host_id, RouterCallback<proto::router::ConnectionOffer> callback);

    // Re-keys the account under |new_password| and hands over every workspace key re-sealed to the
    // new key pair. The router re-runs the 2FA stage afterwards, so a code will be asked again.
    void changePassword(const SecureString& new_password,
                        RouterCallback<proto::router::ChangePasswordResult> callback);

signals:
    void sig_statusChanged(qint64 router_id, Router::Status status);
    void sig_errorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void sig_passwordChangeRequired(qint64 router_id);
    void sig_twoFactorCodeRequired(qint64 router_id);
    void sig_twoFactorEnrollment(qint64 router_id, const QString& otpauth_uri);

    // Everything the session sends, connected to the worker owning the socket.
    void sig_sendMessage(qint64 router_id, quint8 channel_id, const QByteArray& buffer);

    // The server says a list has changed and subscribers should refetch it. Fired at most once per
    // ~5 seconds per resource. A regular client session receives only temp hosts, hosts,
    // workspaces and groups.
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
    // Test-only access to the keys, the pending replies and the incoming messages.
    friend class RouterTestPeer;

    void setStatus(Status status);
    void connectWorker();
    void disconnectWorker();
    void clearSessionState();
    void send(quint8 channel_id, const google::protobuf::MessageLite& message);
    void readUserKeys(const proto::router::UserKeys& user_keys);
    void readTwoFactorChallenge(const proto::router::TwoFactorChallenge& challenge);
    void readTwoFactorResult(const proto::router::TwoFactorResult& result);
    void persistChangedPassword(const SecureString& new_password);
    void emitNotificationSignals(const proto::router::Notification& notification);

    // Delivers a reply to whoever waits for it and feeds the staleness rules of the cache. Returns
    // false for a message that answers no request.
    bool routeReply(const proto::router::RouterToAdmin& message);
    bool routeReply(const proto::router::RouterToManager& message);
    bool routeReply(const proto::router::RouterToClient& message);

    // A complete list (|requested_workspace_id| == 0) also refreshes the cache and the group keys.
    RouterWorkspaceList applyWorkspaceList(const proto::router::WorkspaceList& list,
                                           qint64 requested_workspace_id);

    RouterHostList applyHostList(const proto::router::HostList& list,
                                 const RouterCache::HostKey& key, bool cacheable);
    RouterGroupList applyGroupList(const proto::router::GroupList& list);

    RouterConfig config_;
    QPointer<RouterWorker> router_worker_;
    QVersionNumber version_;
    Status status_ = Status::OFFLINE;

    RouterCache cache_;
    RouterKeys keys_;
    RouterRpc rpc_;

    Q_DISABLE_COPY_MOVE(Router)
};

#endif // CLIENT_ROUTER_H
