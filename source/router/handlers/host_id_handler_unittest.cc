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

#include "router/handlers/host_id_handler.h"

#include "base/crypto/generic_hash.h"
#include "proto/router_host.h"
#include "router/router_test_base.h"
#include "router/workers/client_worker.h"

// The host channel: what a host gets when it asks for its id, and what that leaves in the database.
class HostIdHandlerTest : public RouterTestBase
{
protected:
    void SetUp() override
    {
        RouterTestBase::SetUp();

        peer_.computer_name = "COMPUTER";
        peer_.architecture = "x86_64";
        peer_.version = "3.0.0";
        peer_.os_name = "Windows 11";
        peer_.address = "192.168.1.10";
    }

    HostIdResult handle(const proto::router::HostIdRequest& request,
                        HostId current_host_id = kInvalidHostId, int request_count = 1)
    {
        return handleHostIdRequest(db_, request, peer_, current_host_id, request_count);
    }

    static proto::router::HostIdRequest existingIdRequest(std::string_view key)
    {
        proto::router::HostIdRequest request;
        request.set_type(proto::router::HostIdRequest::EXISTING_ID);
        request.set_key(std::string(key));
        request.set_hw_id("hwid-1");
        return request;
    }

    static proto::router::HostIdRequest newIdRequest()
    {
        proto::router::HostIdRequest request;
        request.set_type(proto::router::HostIdRequest::NEW_ID);
        request.set_hw_id("hwid-1");
        return request;
    }

    // The router stores the hash of the host key, so an approved host is registered by the hash of
    // the key it will present.
    HostId approveHost(std::string_view key)
    {
        const QByteArray key_hash = GenericHash::hash(GenericHash::Type::BLAKE2b512, key);
        if (!db_.addHost(key_hash, "hwid-1"))
            return kInvalidHostId;

        HostId host_id = kInvalidHostId;
        if (db_.hostId(key_hash, &host_id) != proto::router::kErrorOk)
            return kInvalidHostId;
        return host_id;
    }

    qint64 hostCount()
    {
        bool ok = false;
        const qint64 count = db_.hostCount(&ok);
        return ok ? count : -1;
    }

    HostIdPeer peer_;
};

//--------------------------------------------------------------------------------------------------
// A host that nobody approved yet is not written to the database at all: it gets a temporary id
// and a key it can come back with once an administrator approves it.
TEST_F(HostIdHandlerTest, UnapprovedHostGetsATemporaryId)
{
    const HostIdResult result = handle(newIdRequest());

    EXPECT_EQ(result.action, HostIdResult::Action::ISSUE_TEMP_ID);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_TEMP_HOSTS));
    EXPECT_EQ(result.hardware_id, QByteArray("hwid-1"));
    EXPECT_EQ(hostCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// The hardware id identifies the machine behind the key and is what an administrator sees when a
// clone shows up; a host that does not report one is not let in.
TEST_F(HostIdHandlerTest, HostWithoutHardwareIdIsDisconnected)
{
    proto::router::HostIdRequest request = newIdRequest();
    request.clear_hw_id();

    EXPECT_EQ(handle(request).action, HostIdResult::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// The value is opaque to the router and only ever stored, so the one rule about it is a bound.
TEST_F(HostIdHandlerTest, OversizedHardwareIdIsDisconnected)
{
    proto::router::HostIdRequest request = newIdRequest();
    request.set_hw_id(std::string(65, 'x'));

    EXPECT_EQ(handle(request).action, HostIdResult::Action::CLOSE);

    request.set_hw_id(std::string(64, 'x'));
    EXPECT_EQ(handle(request).action, HostIdResult::Action::ISSUE_TEMP_ID);
}

//--------------------------------------------------------------------------------------------------
// A host asks once per session. A repeat would let it swap the id its pending-removal bookkeeping
// is tied to, so it is not answered at all.
TEST_F(HostIdHandlerTest, RepeatedRequestIsIgnored)
{
    const HostId host_id = approveHost("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const HostIdResult result = handle(existingIdRequest("key-1"), host_id);

    EXPECT_EQ(result.action, HostIdResult::Action::IGNORE);
    EXPECT_EQ(result.notify_flags, 0u);
}

//--------------------------------------------------------------------------------------------------
// A host asks for its id once per session and then waits, so a request the router cannot answer
// must not be left unanswered. The session is closed instead, and the host comes back by itself.
TEST_F(HostIdHandlerTest, HostIsDisconnectedWhenTheDatabaseIsUnavailable)
{
    Database unavailable;
    ASSERT_FALSE(unavailable.isValid());

    const HostIdResult result =
        handleHostIdRequest(unavailable, existingIdRequest("key-1"), peer_, kInvalidHostId, 1);

    EXPECT_EQ(result.action, HostIdResult::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// A lookup that finds nothing leaves the session free to ask again, and that is what a host told
// "not found" does. A peer that keeps asking is spending the router, not looking for its id.
TEST_F(HostIdHandlerTest, TooManyRequestsDisconnectTheHost)
{
    EXPECT_EQ(handle(existingIdRequest("key-unknown"), kInvalidHostId, 5).action,
              HostIdResult::Action::SEND_RESPONSE);
    EXPECT_EQ(handle(existingIdRequest("key-unknown"), kInvalidHostId, 6).action,
              HostIdResult::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
// A lookup that fails is not an answer the host can act on, and it asks only once per session. The
// session is closed, the same way an unavailable database closes it.
TEST_F(HostIdHandlerTest, HostIsDisconnectedWhenTheLookupFails)
{
    ASSERT_TRUE(execRaw("DROP TABLE hosts"));

    const HostIdResult result = handle(existingIdRequest("key-1"));

    EXPECT_EQ(result.action, HostIdResult::Action::CLOSE);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostIdHandlerTest, UnknownRequestTypeIsIgnored)
{
    proto::router::HostIdRequest request = newIdRequest();
    request.set_type(static_cast<proto::router::HostIdRequest::Type>(42));

    EXPECT_EQ(handle(request).action, HostIdResult::Action::IGNORE);
}

//--------------------------------------------------------------------------------------------------
// An approved host gets its permanent id back and the connection refreshes what the console shows
// about it.
TEST_F(HostIdHandlerTest, ApprovedHostGetsItsIdAndRefreshesItsTelemetry)
{
    const HostId host_id = approveHost("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const HostIdResult result = handle(existingIdRequest("key-1"));

    EXPECT_EQ(result.action, HostIdResult::Action::SEND_RESPONSE);
    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.host_id, host_id);
    EXPECT_FALSE(result.removal_pending);
    EXPECT_EQ(result.notify_flags, quint32(ClientWorker::NOTIFY_HOSTS));

    const proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.computer_name(), "COMPUTER");
    EXPECT_EQ(stored.display_name(), "COMPUTER");
    EXPECT_EQ(stored.os_name(), "Windows 11");
    EXPECT_EQ(stored.version(), "3.0.0");
    EXPECT_EQ(stored.address(), "192.168.1.10");
    EXPECT_GT(stored.last_connect(), 0);
}

//--------------------------------------------------------------------------------------------------
// The key of a host that was never approved (or whose removal was finalized) is simply not known;
// the host is told so and asks for a new id.
TEST_F(HostIdHandlerTest, UnknownKeyIsRefused)
{
    const HostIdResult result = handle(existingIdRequest("key-unknown"));

    EXPECT_EQ(result.action, HostIdResult::Action::SEND_RESPONSE);
    EXPECT_EQ(result.error_code, proto::router::kErrorNotFound);
    EXPECT_EQ(result.host_id, kInvalidHostId);
    EXPECT_EQ(result.notify_flags, 0u);
    EXPECT_EQ(hostCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// The removal was scheduled while the host was offline. It is recognised - that is what the queued
// row is for - and told to uninstall itself; its connect metadata is deliberately not stored,
// because the record is on its way out.
TEST_F(HostIdHandlerTest, HostScheduledForRemovalIsToldToUninstall)
{
    const HostId host_id = approveHost("key-1");
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    const HostIdResult result = handle(existingIdRequest("key-1"));

    EXPECT_EQ(result.action, HostIdResult::Action::SEND_RESPONSE);
    EXPECT_EQ(result.error_code, proto::router::kErrorOk);
    EXPECT_EQ(result.host_id, host_id);
    EXPECT_TRUE(result.removal_pending);

    // Nothing was resurrected in the hosts table by the connection.
    EXPECT_EQ(hostCount(), 0);
    EXPECT_TRUE(db_.hasPendingHostRemoval(host_id));
}

//--------------------------------------------------------------------------------------------------
// The label an administrator gave the host is not touched by a reconnection, whatever the machine
// calls itself now.
TEST_F(HostIdHandlerTest, ReconnectionKeepsTheAdministratorsLabel)
{
    const HostId host_id = approveHost("key-1");
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_EQ(handle(existingIdRequest("key-1")).error_code, proto::router::kErrorOk);
    ASSERT_TRUE(db_.modifyHost(host_id, 0, "Accounting", std::string_view(), std::string_view(),
                               std::string_view()));

    peer_.computer_name = "RENAMED-BY-OS";
    peer_.address = "10.0.0.5";

    ASSERT_EQ(handle(existingIdRequest("key-1")).error_code, proto::router::kErrorOk);

    const proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.display_name(), "Accounting");
    EXPECT_EQ(stored.computer_name(), "RENAMED-BY-OS");
    EXPECT_EQ(stored.address(), "10.0.0.5");
}
