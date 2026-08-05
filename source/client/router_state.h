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

#ifndef CLIENT_ROUTER_STATE_H
#define CLIENT_ROUTER_STATE_H

#include <QHash>
#include <QPointer>

#include <functional>
#include <typeinfo>
#include <unordered_map>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "base/logging.h"
#include "client/router_types.h"
#include "proto/router_client.h"

namespace proto::router {
class ChangePasswordRequest;
class Group;
class Host;
class RouterToAdmin;
class RouterToManager;
class User;
class UserKeys;
class Workspace;
} // namespace proto::router

// Everything the client knows about a router session except the socket: who we are, the group keys
// we hold, the replies we still wait for and the decoded lists we cached. No networking and no
// database, so the decoding, the key handling and the cache policy are unit-testable; Router keeps
// the transport, the status machine and the persistence.
class RouterState
{
public:
    RouterState() = default;
    ~RouterState() = default;

    enum class KeysResult
    {
        OK,                      // Identity and workspace keys are loaded.
        PASSWORD_CHANGE_REQUIRED, // The record has no wrapped private key yet.
        DECRYPT_FAILED           // The password does not open the stored private key.
    };

    // Identifies a cached host list. The page is part of the identity: two pages of the same
    // selection are different answers, and serving one for the other would show the wrong rows.
    struct HostCacheKey
    {
        qint64 workspace_id = 0;
        qint64 group_id = 0;
        qint64 offset = 0;
        qint64 count = 0;

        bool operator==(const HostCacheKey& other) const = default;

        friend size_t qHash(const HostCacheKey& key, size_t seed = 0)
        {
            return qHashMulti(seed, key.workspace_id, key.group_id, key.offset, key.count);
        }
    };

    //----------------------------------------------------------------------------------------------
    // Session identity and keys.
    //----------------------------------------------------------------------------------------------

    // Takes the identity and the workspace keys of the session from the UserKeys message. A key
    // that cannot be unwrapped is skipped, leaving a hole that no refetch can repair - only a
    // re-grant can.
    KeysResult applyUserKeys(const proto::router::UserKeys& user_keys, const SecureString& password);

    // Drops the identity, the keys and the requests we still wait for. The lists stay: they are
    // dropped by clearCaches() when the session is known to be gone.
    void clearSession();

    // Drops every cached list. Also called when the session is suspended (the router re-opens the
    // two-factor stage), because from that moment nothing guarantees the lists are still current.
    void clearCaches();

    qint64 userId() const { return user_id_; }
    const QString& userName() const { return user_name_; }
    bool hasWorkspaceKey(qint64 workspace_id) const;
    int workspaceKeyCount() const { return static_cast<int>(workspace_cryptors_.size()); }

    //----------------------------------------------------------------------------------------------
    // Requests we still wait for.
    //----------------------------------------------------------------------------------------------

    qint64 nextRequestId() { return ++next_request_id_; }
    int pendingCount() const { return pending_.size(); }
    void clearPending() { pending_.clear(); }

    // Invokes |handler| (either a member-function-pointer of |receiver| or any callable taking
    // const ArgT&) with |arg|.
    template<typename HandlerT, typename ArgT>
    static void invokeHandler(QObject* receiver, HandlerT& handler, const ArgT& arg)
    {
        if constexpr (kIsMemberFn<HandlerT>)
            (static_cast<OwnerClass<HandlerT>*>(receiver)->*handler)(arg);
        else
            handler(arg);
    }

    // Register a response handler keyed by the request's request_id(). The dispatcher passes the
    // parsed response submessage by pointer; the lambda casts it back to ResponseT (the explicit
    // template argument) and invokes |handler|.
    //
    // |handler| can be either a member-function-pointer of |receiver| or any callable taking
    // const ResponseT&. The dispatch branch is selected at compile time via if-constexpr, so
    // every Router::xxx method takes a single HandlerT parameter and forwards it through here
    // without needing per-method overloads.
    template<typename ResponseT, typename RequestT, typename HandlerT>
    void registerPending(const RequestT* request, QObject* receiver, HandlerT handler)
    {
        pending_.emplace(request->request_id(),
            QPointer<QObject>(receiver),
            &typeid(ResponseT),
            [receiver, handler = std::move(handler)](const void* parsed)
        {
            invokeHandler(receiver, handler, *static_cast<const ResponseT*>(parsed));
        });
    }

    // Same as above but with a post-processing step: the dispatcher casts the parsed bytes to
    // RawT, runs them through decoder(), and the result is passed to handler. Used for responses
    // where the wire-level proto differs from what the consumer expects (e.g. workspace list,
    // where the encrypted proto is decoded into a plain struct before delivery).
    template<typename RawT, typename RequestT, typename HandlerT, typename DecoderT>
    void registerPending(const RequestT* request, QObject* receiver,
                         HandlerT handler, DecoderT decoder)
    {
        pending_.emplace(request->request_id(),
            QPointer<QObject>(receiver),
            &typeid(RawT),
            [receiver, handler = std::move(handler), decoder = std::move(decoder)](const void* parsed)
        {
            invokeHandler(receiver, handler, decoder(*static_cast<const RawT*>(parsed)));
        });
    }

    // Look up the pending entry for a given request_id and invoke its handler with the parsed
    // response. Called once per case in the reply dispatcher of Router.
    template<typename ResponseT>
    void dispatch(qint64 request_id, const ResponseT& response)
    {
        Pending pending = pending_.take(request_id);
        if (pending.receiver.isNull())
            return;

        if (!pending.response_type || *pending.response_type != typeid(ResponseT))
        {
            LOG(ERROR) << "Router response type mismatch for request" << request_id;
            return;
        }

        pending.invoke(&response);
    }

    //----------------------------------------------------------------------------------------------
    // Incoming messages.
    //----------------------------------------------------------------------------------------------

    // Delivers a reply to whoever is waiting for it and drops the cached lists that reply has just
    // made stale. Returns false when the message is not a reply to a request: the session-level
    // messages (the two-factor stage, the keys, the change notifications) belong to Router, which
    // owns the status and the persistence.
    //
    // Which lists are dropped mirrors what the router announces as changed for the same operation.
    // The reply arrives at once while its notification is batched and comes seconds later; serving
    // the cache in between would show state that is already gone.
    bool routeReply(const proto::router::RouterToAdmin& message);
    bool routeReply(const proto::router::RouterToManager& message);
    bool routeReply(const proto::router::RouterToClient& message);

    //----------------------------------------------------------------------------------------------
    // Replies: decoding and caching.
    //----------------------------------------------------------------------------------------------

    // Decodes the workspace list and, when it is the complete one (|requested_workspace_id| == 0)
    // and carries no error, refreshes the cache and the set of the group keys we hold.
    RouterWorkspaceList applyWorkspaceList(const proto::router::WorkspaceList& list,
                                           qint64 requested_workspace_id);

    // Decodes the host list and caches it under |key| when |cacheable| and the reply is not an
    // error (caching an error reply would serve its emptiness as a success).
    RouterHostList applyHostList(const proto::router::HostList& list, const HostCacheKey& key,
                                 bool cacheable);

    // Decodes the group list of a workspace and caches it unless the reply is an error.
    RouterGroupList applyGroupList(const proto::router::GroupList& list);

    RouterHostList decodeHostSearchResult(const proto::router::HostSearchResult& result) const;
    RouterTempHostList decodeTempHostList(const proto::router::TempHostList& list) const;

    //----------------------------------------------------------------------------------------------
    // Cached lists.
    //----------------------------------------------------------------------------------------------

    bool workspacesLoaded() const { return workspaces_loaded_; }

    // The cached list as a reply: a network reply always carries an error code, so a handler is
    // allowed to check it and the synthesized one must not look like an error.
    RouterWorkspaceList cachedWorkspaceList() const;

    // Marks the cached workspace list stale without dropping it - the next request refetches.
    void invalidateWorkspaces() { workspaces_loaded_ = false; }

    // Return nullptr when nothing is cached for the key. A present entry means the data was
    // fetched; an empty list is a valid cached value.
    const RouterHostList* cachedHostList(const HostCacheKey& key) const;
    const RouterGroupList* cachedGroupList(qint64 workspace_id) const;

    void clearHostCache() { cached_hosts_.clear(); }
    void clearGroupCache() { cached_groups_.clear(); }

    //----------------------------------------------------------------------------------------------
    // Requests: encoding.
    //----------------------------------------------------------------------------------------------

    bool buildWorkspace(const RouterWorkspace& workspace, proto::router::Workspace* out) const;
    bool buildHost(const RouterHost& host, proto::router::Host* out) const;
    bool buildGroup(qint64 workspace_id, const RouterGroup& group, proto::router::Group* out) const;

    // Re-seals every workspace key we hold to |new_public_key| and appends the results to the
    // message (any proto with a repeated WorkspaceKey workspace_key field).
    void resealGroupKeys(const QByteArray& new_public_key, proto::router::User* user) const;
    void resealGroupKeys(const QByteArray& new_public_key,
                         proto::router::ChangePasswordRequest* request) const;

private:
    struct Pending
    {
        QPointer<QObject> receiver;
        const std::type_info* response_type = nullptr;
        std::function<void(const void* response)> invoke;
    };

    // Type-trait that yields the class that owns a member-function-pointer. Used by
    // invokeHandler() above to downcast the type-erased QObject* |receiver| back to the
    // concrete class before invoking the slot.
    //
    //     OwnerClass<decltype(&Foo::bar)> == Foo
    //
    // Two specializations cover non-const and const member functions; the using-alias
    // strips cv-qualifiers off the pointer-to-member itself (added implicitly when the
    // handler is captured by value in a const lambda).
    template<typename T>
    struct OwnerClassImpl;

    template<typename Ret, typename Class, typename... Args>
    struct OwnerClassImpl<Ret (Class::*)(Args...)>
    {
        using type = Class;
    };

    template<typename Ret, typename Class, typename... Args>
    struct OwnerClassImpl<Ret (Class::*)(Args...) const>
    {
        using type = Class;
    };

    template<typename T>
    using OwnerClass = typename OwnerClassImpl<std::remove_cv_t<T>>::type;

    template<typename T>
    static constexpr bool kIsMemberFn = std::is_member_function_pointer_v<std::remove_cv_t<T>>;

    RouterHost decodeHost(const proto::router::Host& src) const;
    SecureByteArray unwrapGroupKey(const QByteArray& wrapped_gk) const;

    qint64 next_request_id_ = 0;
    QHash<qint64, Pending> pending_;

    qint64 user_id_ = 0;
    QString user_name_;
    SecureByteArray user_private_key_;
    std::unordered_map<qint64, DataCryptor> workspace_cryptors_;

    // Decoded list cache served to the list callers that accept a cached answer.
    bool workspaces_loaded_ = false;
    RouterWorkspaceList cached_workspaces_;
    QHash<qint64, RouterGroupList> cached_groups_;    // key: workspace_id
    QHash<HostCacheKey, RouterHostList> cached_hosts_;

    Q_DISABLE_COPY_MOVE(RouterState)
};

#endif // CLIENT_ROUTER_STATE_H
