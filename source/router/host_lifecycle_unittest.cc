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

    qint64 hostCount()
    {
        bool ok = false;
        const qint64 count = db_.hostCount(&ok);
        return ok ? count : -1;
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
    ASSERT_TRUE(execRaw(QStringLiteral("UPDATE sqlite_sequence SET seq=%1 WHERE name='hosts'")
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
                                   QStringLiteral("3.0.0"), "Windows", "192.168.1.10"));

    proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.display_name(), "COMPUTER");
    EXPECT_EQ(stored.computer_name(), "COMPUTER");
    EXPECT_EQ(stored.os_name(), "Windows");
    EXPECT_EQ(stored.address(), "192.168.1.10");
    EXPECT_GT(stored.last_connect(), 0);

    ASSERT_TRUE(db_.modifyHost(host_id, 0, "Accounting", std::string_view(), std::string_view(),
                               std::string_view()));

    ASSERT_TRUE(db_.updateHostInfo(host_id, "hwid-1", "RENAMED-BY-OS", "x86_64",
                                   QStringLiteral("3.0.1"), "Windows", "192.168.1.11"));

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
                                    QStringLiteral("3.0.0"), "Windows", "127.0.0.1"));
    EXPECT_FALSE(db_.updateHostInfo(kInvalidHostId, "hwid", "COMPUTER", "x86_64",
                                    QStringLiteral("3.0.0"), "Windows", "127.0.0.1"));
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
// A host of a workspace that was removed meanwhile is gone from the snapshot the console still
// holds. Its next save carries the host in the final set and must answer conflict instead of
// resurrecting a record that no longer exists.
TEST_F(HostLifecycleTest, RemovedHostMakesAWorkspaceSaveConflict)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk, {host_id});
    ASSERT_GT(workspace_id, 0);

    ASSERT_TRUE(db_.scheduleHostRemoval(host_id));

    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, gk)}, {host_id}),
              proto::router::kErrorConflict);

    // The same save without the host applies: the console refetched and dropped it.
    EXPECT_EQ(db_.modifyWorkspace(workspace_id, 1, "alpha", std::string_view(),
                                  {accessEntry(admin_, gk)}, {}),
              proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The identity survives everything the workspace does to it: releasing a host clears the fields
// encrypted with the group key, but the host itself keeps its id and its telemetry.
TEST_F(HostLifecycleTest, WorkspaceReleaseKeepsTheIdentity)
{
    ASSERT_TRUE(db_.addHost("key-1", "hwid-1"));
    const HostId host_id = hostIdByKey("key-1");
    ASSERT_TRUE(db_.updateHostInfo(host_id, "hwid-1", "COMPUTER", "x86_64",
                                   QStringLiteral("3.0.0"), "Windows", "192.168.1.10"));

    const SecureByteArray gk(Random::byteArray(32));
    const qint64 workspace_id = addWorkspace(QStringLiteral("alpha"), gk, {host_id});
    ASSERT_GT(workspace_id, 0);
    ASSERT_TRUE(db_.modifyHost(host_id, 0, "Accounting", "comment", "user", "password"));

    ASSERT_EQ(db_.removeWorkspace(workspace_id), proto::router::kErrorOk);

    const proto::router::Host stored = findHost(host_id);
    EXPECT_EQ(stored.host_id(), host_id);
    EXPECT_EQ(stored.workspace_id(), 0);
    EXPECT_EQ(stored.computer_name(), "COMPUTER");
    EXPECT_EQ(stored.display_name(), "Accounting");
    EXPECT_TRUE(stored.comment().empty());
    EXPECT_TRUE(stored.user_name().empty());
    EXPECT_TRUE(stored.password().empty());
}
