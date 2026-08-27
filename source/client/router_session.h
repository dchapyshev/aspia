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

#ifndef CLIENT_ROUTER_SESSION_H
#define CLIENT_ROUTER_SESSION_H

#include <QList>
#include <QPointer>
#include <QVersionNumber>

#include <google/protobuf/message_lite.h>

#include "base/shared_pointer.h"
#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/router_cache.h"
#include "client/router_rpc.h"
#include "client/router_types.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_manager.h"

class RouterWorker;

// A working session with a router. Born from a completed login, it is online by the fact of
// its existence and dies with the connection.
class RouterSession final : public QObject
{
    Q_OBJECT

public:
    enum class CachePolicy
    {
        USE_CACHE, // Return the cached result when available; otherwise fetch and cache it.
        RELOAD     // Always fetch from the server and refresh the cache.
    };

    RouterSession(SharedPointer<RouterConfig> config, qint64 user_id, const QVersionNumber& peer_version,
           QObject* parent = nullptr);
    ~RouterSession() final;

    // Writes into the stored record the credentials this session is to use from now on. Called
    // once the router has accepted their rotation, so the login that follows uses the new ones.
    // False when the record was not updated; the journal of the record tells the operator to
    // set the new password there by hand.
    bool storeCredentials(const QString& user_name, const SecureString& password);

    qint64 userId() const { return user_id_; }

    QVersionNumber version() const { return version_; }
    qint64 routerId() const { return config_->routerId(); }
    const RouterConfig& config() const { return *config_; }

    //----------------------------------------------------------------------------------------------
    // Admin: list queries.
    //----------------------------------------------------------------------------------------------

    void listRelays(RouterCallback<proto::router::RelayList> callback);
    void listClients(qint64 offset, qint64 count, RouterCallback<proto::router::ClientList> callback);

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
    // The page is mandatory as for listHosts; total_count counts every temporary host.
    void listTempHosts(qint64 offset, qint64 count, RouterCallback<RouterTempHostList> callback);

    void checkHostStatus(HostId host_id, RouterCallback<proto::router::HostStatus> callback);

    // Asks for a relay connection offer to the given host.
    void requestConnection(HostId host_id, RouterCallback<proto::router::ConnectionOffer> callback);

    // Rotates the SRP credentials of the account under |new_password|. The router re-runs the
    // 2FA stage afterwards, so a code will be asked again.
    void changePassword(const SecureString& new_password,
                        RouterCallback<proto::router::ChangePasswordResult> callback);

private slots:
    void onMessageReceived(const proto::router::RouterToAdmin& message);
    void onMessageReceived(const proto::router::RouterToManager& message);
    void onMessageReceived(const proto::router::RouterToClient& message);

private:
    friend class RouterController;
    friend class RouterSessionTestPeer;

    void send(quint8 channel_id, const google::protobuf::MessageLite& message);

    // A complete list (|requested_workspace_id| == 0) also refreshes the cache and the group keys.
    RouterWorkspaceList applyWorkspaceList(const proto::router::WorkspaceList& list,
                                           qint64 requested_workspace_id);
    RouterHostList applyHostList(const proto::router::HostList& list,
                                 const RouterCache::HostKey& key, bool cacheable);
    RouterGroupList applyGroupList(const proto::router::GroupList& list);

    SharedPointer<RouterConfig> config_;
    QPointer<RouterWorker> router_worker_;
    QVersionNumber version_;
    qint64 user_id_ = 0;

    RouterCache cache_;
    RouterRpc rpc_;

    Q_DISABLE_COPY_MOVE(RouterSession)
};

#endif // CLIENT_ROUTER_SESSION_H
