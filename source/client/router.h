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
#include <QHash>
#include <QList>
#include <QPointer>
#include <QVersionNumber>

#include <functional>

#include <google/protobuf/message_lite.h>

#include "base/crypto/secure_byte_array.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "base/peer/router_user.h"
#include "base/scoped_qpointer.h"
#include "client/config.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

class QTimer;

// Application-level RPC facade. Owns the TCP channel to the router (on the IO thread, driven
// through queued signals/slots), manages session status (OFFLINE/CONNECTING/ONLINE), holds the
// crypto state derived from UserKeys, and exposes a typed RPC API to GUI consumers.
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

    // Plain (decrypted) workspace data shared between Router and the UI.
    //   * incoming: filled from the decoded server list. access entries carry only user_id;
    //     public_key is always empty (UI has no use for it).
    //   * outgoing: a workspace edit. entry_id == 0 means add, > 0 means modify. For each access
    //     entry public_key is non-empty when the user is being newly granted access (Router
    //     will seal the workspace GK with it) and empty when the user already had access (server
    //     preserves their existing wrapped_gk).
    struct Workspace
    {
        struct Access
        {
            qint64 user_id = 0;
            QByteArray public_key;
        };

        qint64 entry_id = 0;
        QString name;
        QString comment;
        QList<Access> access;
        QList<HostId> host_ids;
    };

    struct WorkspaceList
    {
        QString error_code;
        QList<Workspace> workspaces;
    };

    explicit Router(const RouterConfig& config, QObject* parent = nullptr);
    ~Router() final;

    // Lookup an existing instance by router id. Returns nullptr if no Router with that
    // router_id exists in the current thread.
    static Router* instance(qint64 router_id);

    // Lifecycle.
    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    // Accessors.
    Status status() const { return status_; }
    QVersionNumber version() const { return version_; }
    qint64 routerId() const { return config_.routerId(); }
    const RouterConfig& config() const { return config_; }

    // Admin: list queries.
    template<typename HandlerT>
    void relayList(QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void clientList(QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void userList(QObject* receiver, HandlerT handler);

    // Admin: user operations.
    template<typename HandlerT>
    void userAdd(const proto::router::User& user, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void userModify(const proto::router::User& user, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void userDelete(qint64 entry_id, QObject* receiver, HandlerT handler);

    // Admin: host operations. Pass kAllHostsId to target all hosts.
    template<typename HandlerT>
    void hostDisconnect(HostId host_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void hostRemove(HostId host_id, QObject* receiver, HandlerT handler);

    // Admin: relay/client disconnect. session_id == -1 means "all".
    template<typename HandlerT>
    void relayDisconnect(qint64 session_id, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void clientDisconnect(qint64 session_id, QObject* receiver, HandlerT handler);

    // Admin: workspace operations. Add/modify encrypt the workspace data with the user's key.
    template<typename HandlerT>
    void workspaceAdd(const Router::Workspace& workspace, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void workspaceModify(const Router::Workspace& workspace, QObject* receiver, HandlerT handler);

    template<typename HandlerT>
    void workspaceDelete(qint64 entry_id, QObject* receiver, HandlerT handler);

    // Admin: peer disconnect.
    template<typename HandlerT>
    void peerDisconnect(qint64 relay_id, qint64 peer_id, QObject* receiver, HandlerT handler);

    // Client: workspace list (response is the decoded plain struct). workspace_id == 0
    // returns all visible workspaces; > 0 narrows to a single entry.
    template<typename HandlerT>
    void workspaceList(qint64 workspace_id, QObject* receiver, HandlerT handler);

    // Client: host list. Caller supplies a pre-filled request (mode, filters, etc).
    template<typename HandlerT>
    void hostList(proto::router::HostListRequest request, QObject* receiver, HandlerT handler);

    // Client: ask the router whether a host is currently online.
    template<typename HandlerT>
    void checkHostStatus(HostId host_id, QObject* receiver, HandlerT handler);

    // Client: ask the router for a relay connection offer to the given host.
    template<typename HandlerT>
    void connectionRequest(HostId host_id, QObject* receiver, HandlerT handler);

    // Client: rotate the password of the authenticated user.
    template<typename HandlerT>
    void changePassword(const SecureString& new_password, QObject* receiver, HandlerT handler);

signals:
    void sig_statusChanged(qint64 router_id, Router::Status status);
    void sig_errorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void sig_passwordChangeRequired(qint64 router_id);

private slots:
    void onTcpAuthenticated();
    void onTcpErrorOccurred(TcpChannel::ErrorCode error_code);
    void onTcpMessageReceived(quint8 channel_id, const QByteArray& bytes);
    void onReconnectTimeout();

private:
    struct Pending
    {
        QPointer<QObject> receiver;
        std::function<void(const void* response)> invoke;
    };

    // Type-trait that yields the class that owns a member-function-pointer. Used by
    // invokeHandler() below to downcast the type-erased QObject* |receiver| back to the
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

    // Register a response handler keyed by the request's request_id(). The dispatcher in
    // onRouterReceived passes the parsed response submessage by pointer; the lambda casts it
    // back to ResponseT (the explicit template argument) and invokes |handler|.
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
            [receiver, handler = std::move(handler), decoder = std::move(decoder)](const void* parsed)
        {
            invokeHandler(receiver, handler, decoder(*static_cast<const RawT*>(parsed)));
        });
    }

    // Look up the pending entry for a given request_id and invoke its handler with the parsed
    // response. Called once per case in the onRouterReceived oneof switch.
    template<typename ResponseT>
    void dispatch(qint64 request_id, const ResponseT& response)
    {
        Pending pending = pending_.take(request_id);
        if (pending.receiver.isNull())
            return;
        pending.invoke(&response);
    }

    qint64 nextRequestId() { return ++next_request_id_; }
    void setStatus(Status status);
    bool buildWorkspace(const Router::Workspace& workspace, proto::router::Workspace* out);
    void setupChannel();
    void destroyChannel();
    void emitSend(quint8 channel_id, const google::protobuf::MessageLite& message);
    void readUserKeys(const proto::router::UserKeys& user_keys);
    Router::WorkspaceList decodeWorkspaceList(const proto::router::WorkspaceList& list);
    SecureByteArray unwrapGroupKey(const QByteArray& wrapped_gk) const;

    RouterConfig config_;
    ScopedQPointer<TcpChannel> tcp_channel_;
    QTimer* reconnect_timer_ = nullptr;
    QVersionNumber version_;
    Status status_ = Status::OFFLINE;

    qint64 next_request_id_ = 0;
    QHash<qint64, Pending> pending_;

    qint64 user_id_ = 0;
    QString user_name_;
    SecureByteArray user_private_key_;
    QHash<qint64, SecureByteArray> workspace_group_keys_;

    Q_DISABLE_COPY_MOVE(Router)
};

Q_DECLARE_METATYPE(Router::Workspace)
Q_DECLARE_METATYPE(Router::WorkspaceList)

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::relayList(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_list_request();
    request->set_request_id(nextRequestId());
    registerPending<proto::router::RelayList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::clientList(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_list_request();
    request->set_request_id(nextRequestId());
    registerPending<proto::router::ClientList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::userList(QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_list_request();
    request->set_request_id(nextRequestId());
    registerPending<proto::router::UserList>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::userAdd(const proto::router::User& user, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandUserAdd);
    request->mutable_user()->CopyFrom(user);
    registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::userModify(const proto::router::User& user, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandUserModify);
    request->mutable_user()->CopyFrom(user);
    registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::userDelete(qint64 entry_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_user_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandUserDelete);
    request->mutable_user()->set_entry_id(entry_id);
    registerPending<proto::router::UserResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::hostDisconnect(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandHostDisconnect);
    request->set_host_id(host_id);
    registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::hostRemove(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_host_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandHostRemove);
    request->set_host_id(host_id);
    registerPending<proto::router::HostResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::relayDisconnect(qint64 session_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_relay_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandRelayDisconnect);
    request->set_entry_id(session_id);
    registerPending<proto::router::RelayResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::clientDisconnect(qint64 session_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_client_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandClientDisconnect);
    request->set_entry_id(session_id);
    registerPending<proto::router::ClientResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::workspaceAdd(const Router::Workspace& workspace, QObject* receiver, HandlerT handler)
{
    proto::router::Workspace ws;
    if (!buildWorkspace(workspace, &ws))
        return;
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceAdd);
    request->mutable_workspace()->Swap(&ws);
    registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::workspaceModify(const Router::Workspace& workspace, QObject* receiver, HandlerT handler)
{
    proto::router::Workspace ws;
    if (!buildWorkspace(workspace, &ws))
        return;
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceModify);
    request->mutable_workspace()->Swap(&ws);
    registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::workspaceDelete(qint64 entry_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_workspace_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandWorkspaceDelete);
    request->mutable_workspace()->set_entry_id(entry_id);
    registerPending<proto::router::WorkspaceResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::peerDisconnect(qint64 relay_id, qint64 peer_id, QObject* receiver, HandlerT handler)
{
    proto::router::AdminToRouter message;
    auto* request = message.mutable_peer_request();
    request->set_request_id(nextRequestId());
    request->set_command_name(proto::router::kCommandPeerDisconnect);
    request->set_relay_id(relay_id);
    request->set_peer_id(peer_id);
    registerPending<proto::router::PeerResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_ADMIN, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::workspaceList(qint64 workspace_id, QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_workspace_list_request();
    request->set_request_id(nextRequestId());
    request->set_workspace_id(workspace_id);
    registerPending<proto::router::WorkspaceList>(request, receiver, std::move(handler),
        [this](const proto::router::WorkspaceList& raw)
    {
        return decodeWorkspaceList(raw);
    });
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::hostList(proto::router::HostListRequest request, QObject* receiver, HandlerT handler)
{
    request.set_request_id(nextRequestId());
    proto::router::ClientToRouter message;
    message.mutable_host_list_request()->Swap(&request);
    registerPending<proto::router::HostList>(
        &message.host_list_request(), receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::checkHostStatus(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_check_host_status();
    request->set_request_id(nextRequestId());
    request->set_host_id(host_id);
    registerPending<proto::router::HostStatus>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::connectionRequest(HostId host_id, QObject* receiver, HandlerT handler)
{
    proto::router::ClientToRouter message;
    auto* request = message.mutable_connection_request();
    request->set_request_id(nextRequestId());
    request->set_host_id(host_id);
    registerPending<proto::router::ConnectionOffer>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

//--------------------------------------------------------------------------------------------------
template<typename HandlerT>
void Router::changePassword(const SecureString& new_password, QObject* receiver, HandlerT handler)
{
    RouterUser new_user = RouterUser::create(user_name_, new_password);

    proto::router::ClientToRouter message;
    auto* request = message.mutable_change_password_request();
    request->set_request_id(nextRequestId());
    request->set_salt(new_user.salt.toStdString());
    request->set_verifier(new_user.verifier.toStdString());
    request->set_public_key(new_user.public_key.toStdString());
    request->set_wrap_private_key(new_user.wrap_private_key.toStdString());
    request->set_wrap_salt(new_user.wrap_salt.toStdString());
    registerPending<proto::router::ChangePasswordResult>(request, receiver, std::move(handler));
    emitSend(proto::router::CHANNEL_ID_CLIENT, message);
}

#endif // CLIENT_ROUTER_H
