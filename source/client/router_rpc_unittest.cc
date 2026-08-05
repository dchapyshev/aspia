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

#include "client/router_rpc.h"

#include <QObject>

#include <gtest/gtest.h>

#include "proto/router_admin.h"
#include "proto/router_client.h"

class RouterRpcTest : public testing::Test
{
protected:
    RouterRpc rpc_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterRpcTest, DispatchInvokesTheHandlerOnce)
{
    QObject receiver;
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(rpc_.nextRequestId());
    rpc_.registerPending<proto::router::UserList>(&request, &receiver,
        [&calls](const proto::router::UserList&) { ++calls; });

    EXPECT_EQ(rpc_.pendingCount(), 1);

    proto::router::UserList response;
    response.set_request_id(request.request_id());

    rpc_.dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(rpc_.pendingCount(), 0);

    // A second reply with the same id has nothing to deliver to.
    rpc_.dispatch(request.request_id(), response);
    EXPECT_EQ(calls, 1);
}

//--------------------------------------------------------------------------------------------------
// The receiver is a widget that can be closed while its request is in flight.
TEST_F(RouterRpcTest, DispatchSkipsDestroyedReceiver)
{
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(rpc_.nextRequestId());

    {
        QObject receiver;
        rpc_.registerPending<proto::router::UserList>(&request, &receiver,
            [&calls](const proto::router::UserList&) { ++calls; });
    }

    proto::router::UserList response;
    rpc_.dispatch(request.request_id(), response);

    EXPECT_EQ(calls, 0);
}

//--------------------------------------------------------------------------------------------------
// A reply of another kind carrying the id of a waiting request means the router broke the protocol.
// No real answer is coming after that, so the caller has to be told: a dialog that disabled itself
// for the round trip has nothing else to wake it.
TEST_F(RouterRpcTest, ReplyOfTheWrongKindStillAnswersTheCaller)
{
    QObject receiver;

    proto::router::UserListRequest request;
    request.set_request_id(rpc_.nextRequestId());

    proto::router::UserList delivered;
    bool called = false;

    rpc_.registerPending<proto::router::UserList>(&request, &receiver,
        [&](const proto::router::UserList& list) { delivered = list; called = true; });

    proto::router::HostList wrong_kind;
    wrong_kind.set_request_id(request.request_id());
    rpc_.dispatch(request.request_id(), wrong_kind);

    ASSERT_TRUE(called);
    EXPECT_EQ(delivered.error_code(), proto::router::kErrorLostConnection);
    EXPECT_EQ(rpc_.pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// The decoding step runs before the handler, which is what lets the callers work with the plain
// structs instead of the wire messages. The made-up lost-connection reply goes through the same
// decoder, so a caller always receives the decoded shape.
TEST_F(RouterRpcTest, DecoderRunsBeforeTheHandlerOnBothPaths)
{
    struct Decoded
    {
        QString error_code;
        int hosts = 0;
    };

    const auto decoder = [](const proto::router::HostList& raw)
    {
        Decoded decoded;
        decoded.error_code = QString::fromStdString(raw.error_code());
        decoded.hosts = raw.host_size();
        return decoded;
    };

    QObject receiver;
    Decoded delivered;

    proto::router::HostListRequest request;
    request.set_request_id(rpc_.nextRequestId());
    rpc_.registerPending<proto::router::HostList>(&request, &receiver,
        [&delivered](const Decoded& decoded) { delivered = decoded; }, decoder);

    proto::router::HostList response;
    response.set_error_code(proto::router::kErrorOk);
    response.add_host();
    rpc_.dispatch(request.request_id(), response);

    EXPECT_EQ(delivered.error_code, QString::fromUtf8(proto::router::kErrorOk));
    EXPECT_EQ(delivered.hosts, 1);

    // And the failure path of the same registration.
    proto::router::HostListRequest failed_request;
    failed_request.set_request_id(rpc_.nextRequestId());
    rpc_.registerPending<proto::router::HostList>(&failed_request, &receiver,
        [&delivered](const Decoded& decoded) { delivered = decoded; }, decoder);

    rpc_.clearPending();

    EXPECT_EQ(delivered.error_code, QString::fromUtf8(proto::router::kErrorLostConnection));
    EXPECT_EQ(delivered.hosts, 0);
}

//--------------------------------------------------------------------------------------------------
// Every reply says "no" the same way, whatever kind it is - a connection offer included, whose
// fabricated shape must never read as an offer to connect.
TEST_F(RouterRpcTest, ALostSessionAnswersEveryKindOfCaller)
{
    QObject receiver;

    proto::router::HostRequest host_request;
    host_request.set_request_id(rpc_.nextRequestId());
    proto::router::HostResult host_result;
    rpc_.registerPending<proto::router::HostResult>(&host_request, &receiver,
        [&](const proto::router::HostResult& result) { host_result = result; });

    proto::router::ConnectionRequest offer_request;
    offer_request.set_request_id(rpc_.nextRequestId());
    proto::router::ConnectionOffer offer;
    rpc_.registerPending<proto::router::ConnectionOffer>(&offer_request, &receiver,
        [&](const proto::router::ConnectionOffer& result) { offer = result; });

    rpc_.clearPending();

    EXPECT_EQ(host_result.error_code(), proto::router::kErrorLostConnection);
    EXPECT_EQ(offer.error_code(), proto::router::kErrorLostConnection);
    EXPECT_EQ(rpc_.pendingCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A caller answered by the teardown can fire the next request from inside its handler; the new
// entry must survive the sweep that is still running.
TEST_F(RouterRpcTest, CallerAnsweredByTeardownCanRegisterAgain)
{
    QObject receiver;
    int calls = 0;

    proto::router::UserListRequest request;
    request.set_request_id(rpc_.nextRequestId());
    rpc_.registerPending<proto::router::UserList>(&request, &receiver,
        [&](const proto::router::UserList&)
    {
        ++calls;

        proto::router::UserListRequest retry;
        retry.set_request_id(rpc_.nextRequestId());
        rpc_.registerPending<proto::router::UserList>(&retry, &receiver,
            [&calls](const proto::router::UserList&) { ++calls; });
    });

    rpc_.clearPending();

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(rpc_.pendingCount(), 1);
}
