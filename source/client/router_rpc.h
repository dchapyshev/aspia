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

#ifndef CLIENT_ROUTER_RPC_H
#define CLIENT_ROUTER_RPC_H

#include <QHash>
#include <QPointer>

#include <functional>
#include <typeinfo>

#include "base/logging.h"
#include "proto/router_constants.h"

// Correlates the requests sent to a router with the callers waiting for the answers: hands out
// the request ids, remembers who waits for which id and delivers the parsed reply to it. Knows
// nothing about what the messages mean - decoding and caching come in with the handler.
//
// A caller whose answer can never arrive (the session died, or the router replied with a message
// of another kind) is given a made-up reply carrying kErrorLostConnection, so a dialog that
// disabled itself for the round trip always wakes up.
class RouterRpc
{
public:
    RouterRpc() = default;
    ~RouterRpc() = default;

    qint64 nextRequestId() { return ++next_request_id_; }
    int pendingCount() const { return pending_.size(); }

    // Answers every caller still waiting with a made-up lost-connection reply.
    void clearPending()
    {
        // Taken out first because a caller being answered can start a new request right away.
        const QHash<qint64, Pending> pending = std::move(pending_);
        pending_.clear();

        for (const Pending& entry : pending)
        {
            if (!entry.receiver.isNull() && entry.fail)
                entry.fail();
        }
    }

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
    // every request method takes a single HandlerT parameter and forwards it through here
    // without needing per-method overloads.
    template<typename ResponseT, typename RequestT, typename HandlerT>
    void registerPending(const RequestT* request, QObject* receiver, HandlerT handler)
    {
        pending_.emplace(request->request_id(),
            QPointer<QObject>(receiver),
            &typeid(ResponseT),
            [receiver, handler](const void* parsed)
        {
            invokeHandler(receiver, handler, *static_cast<const ResponseT*>(parsed));
        },
            [receiver, handler = std::move(handler)]
        {
            // The reply made up for a caller whose request died with the session. Every reply
            // carries its error code as a string; a response that cannot say "no" this way would
            // be fabricated as a success, so it does not compile instead.
            ResponseT response;
            response.set_error_code(proto::router::kErrorLostConnection);
            invokeHandler(receiver, handler, response);
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
            [receiver, handler, decoder](const void* parsed)
        {
            invokeHandler(receiver, handler, decoder(*static_cast<const RawT*>(parsed)));
        },
            [receiver, handler = std::move(handler), decoder = std::move(decoder)]
        {
            RawT response;
            response.set_error_code(proto::router::kErrorLostConnection);
            invokeHandler(receiver, handler, decoder(response));
        });
    }

    // Look up the pending entry for a given request_id and invoke its handler with the parsed
    // response. Called once per case in the reply dispatcher.
    template<typename ResponseT>
    void dispatch(qint64 request_id, const ResponseT& response)
    {
        Pending pending = pending_.take(request_id);
        if (pending.receiver.isNull())
            return;

        if (!pending.response_type || *pending.response_type != typeid(ResponseT))
        {
            // The router answered with a reply of another kind, so nothing it sends later belongs
            // to this request either. The caller is told the same way a lost session tells it.
            LOG(ERROR) << "Router response type mismatch for request" << request_id;

            if (pending.fail)
                pending.fail();

            return;
        }

        pending.invoke(&response);
    }

private:
    struct Pending
    {
        QPointer<QObject> receiver;
        const std::type_info* response_type = nullptr;
        std::function<void(const void* response)> invoke;
        std::function<void()> fail;
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

    qint64 next_request_id_ = 0;
    QHash<qint64, Pending> pending_;

    Q_DISABLE_COPY_MOVE(RouterRpc)
};

#endif // CLIENT_ROUTER_RPC_H
