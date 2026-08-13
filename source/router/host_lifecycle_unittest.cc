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

#include "router/router_test_base.h"

// The life of a host record as the admin commands drive it: the approval that creates the identity,
// the telemetry of every connection, and the removal queue that keeps the id alive until the host
// itself acknowledges that it is gone. The commands run in the host worker against live sessions;
// what they leave behind is checked here.
class HostLifecycleTest : public RouterTestBase
{
protected:
    HostId hostIdByKey(std::string_view key_hash, std::string_view* error_code = nullptr)
    {
        HostId host_id = kInvalidHostId;
        const std::string_view code = db_.hostId(key_hash, &host_id);
        if (error_code)
            *error_code = code;
        return host_id;
    }

    // Moves the moment a removal was queued |days| into the past.
    bool ageRemoval(HostId host_id, int days)
    {
        return execRaw(QString("UPDATE hosts_remove SET timestamp=timestamp-%1 WHERE host_id=%2")
                           .arg(qint64(days) * 24 * 3600).arg(host_id));
    }

    qint64 hostCount()
    {
        qint64 count = 0;
        if (db_.hostCount(&count) != proto::router::kErrorOk)
            return -1;
        return count;
    }
};

//--------------------------------------------------------------------------------------------------
// Approving a host is what creates its durable identity: from then on the same key always yields
// the same dialable id.
TEST_F(HostLifecycleTest, ApprovalCreatesADurableIdentity)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));

    std::string_view error_code;
    const HostId host_id = hostIdByKey("key-1", &error_code);

    EXPECT_EQ(error_code, proto::router::kErrorOk);
    EXPECT_NE(host_id, kInvalidHostId);
    EXPECT_FALSE(isTempHostId(host_id));
    EXPECT_EQ(hostIdByKey("key-1"), host_id);
}

//--------------------------------------------------------------------------------------------------
// The key is the identity: approving the same one twice must not produce a second record the host
// could never reach.
TEST_F(HostLifecycleTest, SecondApprovalOfTheSameKeyIsRefused)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    EXPECT_FALSE(db_.addHost("key-1", "hwid-2"));
    EXPECT_EQ(hostCount(), 1);
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostLifecycleTest, ApprovalRequiresKeyAndHardwareId)
{
    EXPECT_FALSE(db_.addHost(std::string_view(), "hwid-1"));
    EXPECT_FALSE(db_.addHost("key-1", std::string_view()));
    EXPECT_EQ(hostCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host that was never approved is not an error - it is the answer that sends the host down the
// temporary-id path.
TEST_F(HostLifecycleTest, UnknownKeyIsNotFound)
{
    std::string_view error_code;
    EXPECT_EQ(hostIdByKey("key-unknown", &error_code), kInvalidHostId);
    EXPECT_EQ(error_code, proto::router::kErrorNotFound);

    EXPECT_EQ(hostIdByKey(std::string_view(), &error_code), kInvalidHostId);
    EXPECT_EQ(error_code, proto::router::kErrorInvalidData);
}

//--------------------------------------------------------------------------------------------------
// Permanent ids and temporary ids live in separate ranges; a permanent id that reached the
// temporary range would collide with a session id of an unapproved host.
TEST_F(HostLifecycleTest, PermanentIdsNeverEnterTheTemporaryRange)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));

    // The next AUTOINCREMENT value would be the first temporary id.
    ASSERT_TRUE(execRaw(QString("UPDATE sqlite_sequence SET seq=%1 WHERE name='hosts'")
                            .arg(kMinTempHostId - 1)));

    EXPECT_FALSE(db_.addHost("key-2", "hwid-2"));
    EXPECT_EQ(hostCount(), 1);
}

//--------------------------------------------------------------------------------------------------
// The label of a host is seeded from its computer name so it is readable right away, but once an
// administrator has named it, no reconnection may overwrite that.
TEST_F(HostLifecycleTest, TelemetrySeedsTheLabelOnlyWhileItIsEmpty)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_TRUE(db_.updateHostInfo(host_id, "hwid-1", "COMPUTER", "x86_64",
                                   "3.0.0", "Windows", "192.168.1.10"));

    proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.display_name(), "COMPUTER");
    EXPECT_EQ(stored.computer_name(), "COMPUTER");
    EXPECT_EQ(stored.os_name(), "Windows");
    EXPECT_EQ(stored.address(), "192.168.1.10");
    EXPECT_GT(stored.last_connect(), 0);

    ASSERT_EQ(db_.modifyHost(host_id, findHost(host_id).revision(), 0, 0, "Accounting",
                             std::string_view()),
              proto::router::kErrorOk);

    ASSERT_TRUE(db_.updateHostInfo(host_id, "hwid-1", "RENAMED-BY-OS", "x86_64",
                                   "3.0.1", "Windows", "192.168.1.11"));

    stored = findHost(host_id);
    EXPECT_EQ(stored.display_name(), "Accounting");
    EXPECT_EQ(stored.computer_name(), "RENAMED-BY-OS");
    EXPECT_EQ(stored.version(), "3.0.1");
}

//--------------------------------------------------------------------------------------------------
// The record is gone (its removal was scheduled while the host was connecting): reporting success
// would hide that from the session.
TEST_F(HostLifecycleTest, TelemetryOfAMissingHostFails)
{
    EXPECT_FALSE(db_.updateHostInfo(HostId(12345), "hwid", "COMPUTER", "x86_64",
                                    "3.0.0", "Windows", "127.0.0.1"));
    EXPECT_FALSE(db_.updateHostInfo(kInvalidHostId, "hwid", "COMPUTER", "x86_64",
                                    "3.0.0", "Windows", "127.0.0.1"));
}

//--------------------------------------------------------------------------------------------------
// Scheduling a removal takes the host out of every list at once, but keeps its id alive: the host
// must still be recognised when it reconnects, so that it can be told to uninstall itself.
TEST_F(HostLifecycleTest, ScheduledRemovalHidesTheHostButKeepsItsId)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);
    ASSERT_EQ(hostCount(), 1);

    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    EXPECT_EQ(hostCount(), 0);
    EXPECT_EQ(findHost(host_id).host_id(), 0u);
    EXPECT_TRUE(db_.hasPendingHostRemoval(host_id));

    std::string_view error_code;
    EXPECT_EQ(hostIdByKey("key-1", &error_code), host_id);
    EXPECT_EQ(error_code, proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The host acknowledged the removal: only now does the identity disappear, and a reconnect starts
// over as an unapproved host.
TEST_F(HostLifecycleTest, FinalizationDropsTheIdentity)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    ASSERT_TRUE(db_.finalizeHostRemoval(host_id));

    EXPECT_FALSE(db_.hasPendingHostRemoval(host_id));

    std::string_view error_code;
    EXPECT_EQ(hostIdByKey("key-1", &error_code), kInvalidHostId);
    EXPECT_EQ(error_code, proto::router::kErrorNotFound);

    // A duplicate acknowledgement (or another session that finalized first) is not a failure: the
    // desired end state is already there.
    EXPECT_TRUE(db_.finalizeHostRemoval(host_id));
}

//--------------------------------------------------------------------------------------------------
TEST_F(HostLifecycleTest, RemovalOfAnUnknownHostFails)
{
    EXPECT_FALSE(db_.scheduleHostRemoval(HostId(12345)));
    EXPECT_FALSE(db_.scheduleHostRemoval(kInvalidHostId));
}

//--------------------------------------------------------------------------------------------------
// The host never came back to acknowledge, and meanwhile it was approved again: the second removal
// must not be blocked by the stale row of the first one, or the host would become un-removable.
TEST_F(HostLifecycleTest, RemovalAfterReapprovalReplacesTheStaleRow)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId first_id = hostIdByKey("key-1");
    ASSERT_TRUE(db_.scheduleHostRemoval(first_id));

    // Approved again under a new identity while the old removal is still pending.
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId second_id = hostIdByKey("key-1");
    EXPECT_NE(second_id, first_id);

    ASSERT_TRUE(db_.scheduleHostRemoval(second_id));

    EXPECT_TRUE(db_.hasPendingHostRemoval(second_id));
    EXPECT_FALSE(db_.hasPendingHostRemoval(first_id));
    EXPECT_EQ(hostCount(), 0);
}

//--------------------------------------------------------------------------------------------------
// A host whose removal is queued is out of the hosts table already. Claiming it for a workspace
// must not resurrect a record that no longer exists.
TEST_F(HostLifecycleTest, RemovedHostCannotBeClaimed)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    EXPECT_EQ(moveHost(host_id, workspace_id), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// The identity survives everything the workspace does to it. Releasing a host clears what it
// carried inside the workspace, but the host keeps its id and its telemetry.
TEST_F(HostLifecycleTest, WorkspaceReleaseKeepsTheIdentity)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_TRUE(db_.updateHostInfo(host_id, "hwid-1", "COMPUTER", "x86_64",
                                   "3.0.0", "Windows", "192.168.1.10"));

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);
    ASSERT_EQ(db_.modifyHost(host_id, findHost(host_id).revision(), workspace_id, 0,
                             "Accounting", "comment"),
              proto::router::kErrorOk);

    ASSERT_EQ(db_.removeWorkspace(workspace_id), proto::router::kErrorOk);

    const proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.host_id(), host_id);
    EXPECT_EQ(stored.workspace_id(), 0);
    EXPECT_EQ(stored.computer_name(), "COMPUTER");
    EXPECT_EQ(stored.display_name(), "Accounting");
    EXPECT_TRUE(stored.comment().empty());
}

//--------------------------------------------------------------------------------------------------
// A removal nobody ever acknowledged does not sit in the queue forever: after the grace period the
// record is dropped, and with it the id. The machine behind it has to be approved anew.
TEST_F(HostLifecycleTest, UnacknowledgedRemovalExpires)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));
    ASSERT_TRUE(db_.hasPendingHostRemoval(host_id));

    // One day past the grace period.
    ASSERT_TRUE(ageRemoval(host_id, 181));

    EXPECT_TRUE(db_.pruneExpiredHostRemovals());
    EXPECT_FALSE(db_.hasPendingHostRemoval(host_id));

    // The key means nothing to the router any more.
    std::string_view error_code;
    EXPECT_EQ(hostIdByKey("key-1", &error_code), kInvalidHostId);
    EXPECT_EQ(error_code, proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// Inside the grace period the host is still expected back: the id is kept, so a machine that spends
// months switched off is still told to remove itself when it returns.
TEST_F(HostLifecycleTest, RemovalInsideTheGracePeriodIsKept)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));
    ASSERT_TRUE(ageRemoval(host_id, 179));

    EXPECT_TRUE(db_.pruneExpiredHostRemovals());
    EXPECT_TRUE(db_.hasPendingHostRemoval(host_id));
    EXPECT_EQ(hostIdByKey("key-1"), host_id);
}

//--------------------------------------------------------------------------------------------------
// The sweep takes the expired ones and leaves the rest alone.
TEST_F(HostLifecycleTest, SweepTakesOnlyTheExpiredRemovals)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    ASSERT_TRUE(db_.addHost("key-2", "hwid-2"));

    const HostId expired_id = hostIdByKey("key-1");
    const HostId fresh_id = hostIdByKey("key-2");
    ASSERT_NE(expired_id, kInvalidHostId);
    ASSERT_NE(fresh_id, kInvalidHostId);

    ASSERT_TRUE(db_.scheduleHostRemoval(expired_id));
    ASSERT_TRUE(db_.scheduleHostRemoval(fresh_id));
    ASSERT_TRUE(ageRemoval(expired_id, 200));

    EXPECT_TRUE(db_.pruneExpiredHostRemovals());

    EXPECT_FALSE(db_.hasPendingHostRemoval(expired_id));
    EXPECT_TRUE(db_.hasPendingHostRemoval(fresh_id));
    EXPECT_EQ(countRaw("SELECT COUNT(*) FROM hosts_remove"), 1);
}
