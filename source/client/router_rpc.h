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
#include <type_traits>
#include <typeinfo>

#include "base/logging.h"
#include "proto/router_constants.h"

// The answer to a request: what to run and the object it belongs to. Nothing is delivered once
// that object is gone, so a handler holding a pointer to it is safe to keep here.
template<typename T>
class RouterCallback
{
public:
    // A member function of |receiver|, bound here so the call sites do not have to.
    template<typename ReceiverT, typename ClassT,
             typename = std::enable_if_t<std::is_base_of_v<ClassT, ReceiverT>>>
    RouterCallback(ReceiverT* receiver, void (ClassT::*method)(const T&))
        : receiver_(receiver),
          handler_(std::bind_front(method, receiver))
    {
        // Nothing
    }

    template<typename CallableT,
             typename = std::enable_if_t<std::is_invocable_v<CallableT, const T&>>>
    RouterCallback(QObject* receiver, CallableT callable)
        : receiver_(receiver),
          handler_(std::move(callable))
    {
        // Nothing
    }

    QObject* receiver() const { return receiver_; }

    void operator()(const T& value) const { handler_(value); }

private:
    QPointer<QObject> receiver_;
    std::function<void(const T&)> handler_;
};

// Hands out the request ids and remembers who waits for which of them. Knows nothing about what
// the messages mean - decoding and caching come in with the handler.
//
// A caller whose answer can never arrive is given a made-up reply carrying kErrorLostConnection,
// so a dialog that disabled itself for the round trip always wakes up.
class RouterRpc
{
public:
    RouterRpc() = default;
    ~RouterRpc() = default;

    qint64 nextRequestId() { return ++next_request_id_; }
    int pendingCount() const { return pending_.size(); }
    void dropPending() { pending_.clear(); }

    // Answers every caller still waiting with a made-up lost-connection reply.
    void clearPending()
    {
        // Taken out first because a caller being answered can start a new request right away.
        QHash<qint64, Pending> pending;
        pending.swap(pending_);

        for (const Pending& entry : pending)
        {
            if (!entry.receiver.isNull() && entry.fail)
                entry.fail();
        }
    }

    // Registers |callback| under the request_id() of |request|. The dispatcher passes the parsed
    // reply by pointer, so the type it is cast back to comes from the callback.
    template<typename ResponseT, typename RequestT>
    void registerPending(const RequestT* request, RouterCallback<ResponseT> callback)
    {
        // Taken out first: as arguments of emplace() the order of evaluation would be unspecified,
        // and the move would race the copy for the callback.
        const QPointer<QObject> receiver(callback.receiver());

        auto invoke = [callback](const void* parsed)
        {
            callback(*static_cast<const ResponseT*>(parsed));
        };

        auto fail = [callback = std::move(callback)]
        {
            // A response with no error code of its own would be fabricated as a success, so it
            // does not compile instead.
            ResponseT response;
            response.set_error_code(proto::router::kErrorLostConnection);
            callback(response);
        };

        pending_.emplace(request->request_id(), receiver, &typeid(ResponseT),
                         std::move(invoke), std::move(fail));
    }

    // Same as above, with |decoder| between the parsed RawT and the callback. For the replies the
    // caller sees as something else than the wire message (an encrypted list as a plain struct).
    template<typename RawT, typename ResultT, typename RequestT, typename DecoderT>
    void registerPending(const RequestT* request, RouterCallback<ResultT> callback,
                         DecoderT decoder)
    {
        // See above: sequenced so the moves cannot precede the copies.
        const QPointer<QObject> receiver(callback.receiver());

        auto invoke = [callback, decoder](const void* parsed)
        {
            callback(decoder(*static_cast<const RawT*>(parsed)));
        };

        auto fail = [callback = std::move(callback), decoder = std::move(decoder)]
        {
            RawT response;
            response.set_error_code(proto::router::kErrorLostConnection);
            callback(decoder(response));
        };

        pending_.emplace(request->request_id(), receiver, &typeid(RawT),
                         std::move(invoke), std::move(fail));
    }

    // Delivers |response| to whoever waits for |request_id|.
    template<typename ResponseT>
    void dispatch(qint64 request_id, const ResponseT& response)
    {
        Pending pending = pending_.take(request_id);
        if (pending.receiver.isNull())
            return;

        if (!pending.response_type || *pending.response_type != typeid(ResponseT))
        {
            // The answer came as a reply of another kind, so nothing later belongs to this
            // request either. The caller is told as a lost session tells it.
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

    qint64 next_request_id_ = 0;
    QHash<qint64, Pending> pending_;

    Q_DISABLE_COPY_MOVE(RouterRpc)
};

#endif // CLIENT_ROUTER_RPC_H
