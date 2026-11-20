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

#include "router/host_store.h"

#include "router/router_test_base.h"

class HostStoreTest : public RouterTestBase
{
protected:
    // The fields a host reports about itself when it connects.
    bool setHostInfo(HostId host_id, std::string_view version, std::string_view address)
    {
        return hosts_.updateInfo(host_id, "hwid", "computer", "x86_64", version, "Windows 11", address);
    }
};

//--------------------------------------------------------------------------------------------------
// The telemetry of a host reads back as it was stored, and a host the router does not know is told
// apart from one that has not reported anything yet.
TEST_F(HostStoreTest, HostTelemetryReadsBack)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    std::string telemetry;
    EXPECT_EQ(hosts_.telemetry(host_id, &telemetry), proto::router::kErrorOk);
    EXPECT_TRUE(telemetry.empty());

    ASSERT_TRUE(hosts_.updateTelemetry(host_id, "{\"version\":1}"));
    EXPECT_EQ(hosts_.telemetry(host_id, &telemetry), proto::router::kErrorOk);
    EXPECT_EQ(telemetry, "{\"version\":1}");

    EXPECT_EQ(hosts_.telemetry(HostId(12345), &telemetry), proto::router::kErrorNotFound);
    EXPECT_TRUE(telemetry.empty());
}

//--------------------------------------------------------------------------------------------------
// A negative group_id asks for the hosts of the workspace whatever group they sit in.
TEST_F(HostStoreTest, HostListTakesEveryGroupOfTheWorkspace)
{
    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(groups_.add(workspace_id, 0, "group", std::string_view(), &group_id), proto::router::kErrorOk);

    const HostId root_host = addHost("hash-1");
    const HostId group_host = addHost("hash-2");
    ASSERT_NE(root_host, kInvalidHostId);
    ASSERT_NE(group_host, kInvalidHostId);

    ASSERT_EQ(moveHost(root_host, workspace_id), proto::router::kErrorOk);
    ASSERT_EQ(moveHost(group_host, workspace_id, group_id), proto::router::kErrorOk);

    qint64 count = 0;
    ASSERT_EQ(hosts_.count(workspace_id, -1, &count), proto::router::kErrorOk);
    EXPECT_EQ(count, 2);

    proto::router::HostList list;
    hosts_.list(workspace_id, -1, 0, proto::router::kMaxHostPageSize, &list);
    ASSERT_EQ(list.error_code(), proto::router::kErrorOk);
    EXPECT_EQ(list.host_size(), 2);

    ASSERT_EQ(hosts_.count(workspace_id, 0, &count), proto::router::kErrorOk);
    EXPECT_EQ(count, 1);
}

//--------------------------------------------------------------------------------------------------
// The pages of the host list tile the table: glued back together they give every row exactly
// once, in the contractual id order, so a row cannot land on two pages or fall between them.
TEST_F(HostStoreTest, HostPagesTileTheTable)
{
    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    QList<HostId> ids;
    for (int i = 0; i < 5; ++i)
    {
        const HostId host_id = addHost("hash-" + std::to_string(i));
        ASSERT_NE(host_id, kInvalidHostId);
        ASSERT_EQ(moveHost(host_id, workspace_id), proto::router::kErrorOk);
        ids.append(host_id);
    }

    auto collect = [](const proto::router::HostList& list)
    {
        QList<HostId> out;
        for (int i = 0; i < list.host_size(); ++i)
            out.append(list.host(i).host_id());
        return out;
    };

    QList<HostId> glued;
    for (qint64 offset = 0; offset < ids.size(); offset += 2)
    {
        proto::router::HostList page;
        hosts_.list(proto::router::HostFilter(), offset, 2, &page);
        ASSERT_EQ(page.error_code(), proto::router::kErrorOk);
        glued.append(collect(page));
    }
    EXPECT_EQ(glued, ids);

    glued.clear();
    for (qint64 offset = 0; offset < ids.size(); offset += 2)
    {
        proto::router::HostList page;
        hosts_.list(workspace_id, -1, offset, 2, &page);
        ASSERT_EQ(page.error_code(), proto::router::kErrorOk);
        glued.append(collect(page));
    }
    EXPECT_EQ(glued, ids);
}

//--------------------------------------------------------------------------------------------------
// The values of the filter are bound ahead of the page, so a page past the first one is where a
// wrong order of the parameters shows. The count covers the matches only.
TEST_F(HostStoreTest, FilteredHostPagesTileTheMatches)
{
    QList<HostId> matches;
    for (int i = 0; i < 7; ++i)
    {
        const HostId host_id = addHost("hash-" + std::to_string(i));
        ASSERT_NE(host_id, kInvalidHostId);

        const bool matching = i % 2 == 0;
        ASSERT_TRUE(setHostInfo(host_id, matching ? "3.0.4" : "3.0.3", "10.0.0." + std::to_string(i)));

        if (matching)
            matches.append(host_id);
    }

    proto::router::HostFilter filter;
    filter.set_version("3.0.4");

    qint64 count = 0;
    ASSERT_EQ(hosts_.count(filter, &count), proto::router::kErrorOk);
    EXPECT_EQ(count, matches.size());

    QList<HostId> glued;
    for (qint64 offset = 0; offset < matches.size(); offset += 3)
    {
        proto::router::HostList page;
        hosts_.list(filter, offset, 3, &page);
        ASSERT_EQ(page.error_code(), proto::router::kErrorOk);

        for (const proto::router::Host& host : page.host())
            glued.append(host.host_id());
    }
    EXPECT_EQ(glued, matches);
}

//--------------------------------------------------------------------------------------------------
// list() and count() refuse a bad filter on their own, whatever the caller checked before.
TEST_F(HostStoreTest, HostsRefusesAnInvalidFilter)
{
    ASSERT_NE(addHost("hash-1"), kInvalidHostId);

    proto::router::HostFilter at_limit;
    for (int i = 0; i < proto::router::kMaxHostFilterWorkspaces; ++i)
        at_limit.add_workspace_id(i);

    const std::string text_at_limit(proto::router::kMaxHostQueryLength, 'a');
    at_limit.set_address(text_at_limit);
    at_limit.set_os_name(text_at_limit);
    at_limit.set_version(text_at_limit);
    at_limit.set_cpu_arch(text_at_limit);
    at_limit.set_display_name(QString(proto::router::kMaxHostQueryLength, QChar(0x0410)).toStdString());
    at_limit.set_host_id(std::string(proto::router::kMaxHostIdLength, '9'));

    proto::router::HostList accepted;
    hosts_.list(at_limit, 0, proto::router::kMaxHostPageSize, &accepted);
    EXPECT_EQ(accepted.error_code(), proto::router::kErrorOk);

    proto::router::HostFilter over_limit = at_limit;
    over_limit.add_workspace_id(proto::router::kMaxHostFilterWorkspaces);

    proto::router::HostFilter negative_workspace;
    negative_workspace.add_workspace_id(-1);

    const std::string long_text(proto::router::kMaxHostQueryLength + 1, 'a');

    proto::router::HostFilter long_address;
    long_address.set_address(long_text);

    proto::router::HostFilter long_os_name;
    long_os_name.set_os_name(long_text);

    proto::router::HostFilter long_version;
    long_version.set_version(long_text);

    proto::router::HostFilter long_cpu_arch;
    long_cpu_arch.set_cpu_arch(long_text);

    proto::router::HostFilter long_display_name;
    long_display_name.set_display_name(long_text);

    proto::router::HostFilter long_host_id;
    long_host_id.set_host_id(std::string(proto::router::kMaxHostIdLength + 1, '1'));

    proto::router::HostFilter non_digit_host_id;
    non_digit_host_id.set_host_id("12a");

    for (const proto::router::HostFilter& filter : { over_limit, negative_workspace, long_address, long_os_name,
                                                     long_version, long_cpu_arch, long_display_name, long_host_id,
                                                     non_digit_host_id })
    {
        proto::router::HostList list;
        hosts_.list(filter, 0, proto::router::kMaxHostPageSize, &list);
        EXPECT_EQ(list.error_code(), proto::router::kErrorInvalidRequest);
        EXPECT_EQ(list.host_size(), 0);

        qint64 count = 42;
        EXPECT_EQ(hosts_.count(filter, &count), proto::router::kErrorInvalidRequest);
        EXPECT_EQ(count, 0);
    }
}

//--------------------------------------------------------------------------------------------------
// The address is a substring typed by the user. A LIKE wildcard or the escape character in it
// matches only itself.
TEST_F(HostStoreTest, HostFilterMatchesTheAddressLiterally)
{
    const HostId plain = addHost("hash-1");
    const HostId special = addHost("hash-2");
    ASSERT_NE(plain, kInvalidHostId);
    ASSERT_NE(special, kInvalidHostId);

    ASSERT_TRUE(setHostInfo(plain, "3.0.4", "192.168.1.10"));
    ASSERT_TRUE(setHostInfo(special, "3.0.4", "a\\b%c_d"));

    const struct { const char* address; qint64 count; } kCases[] = {
        { "\\", 1 },
        { "%", 1 },
        { "_", 1 },
        { "b%c", 1 },
        { "192_168", 0 },
        { "192.168", 1 }
    };

    for (const auto& test_case : kCases)
    {
        proto::router::HostFilter filter;
        filter.set_address(test_case.address);

        qint64 count = 0;
        ASSERT_EQ(hosts_.count(filter, &count), proto::router::kErrorOk) << test_case.address;
        EXPECT_EQ(count, test_case.count) << test_case.address;

        proto::router::HostList list;
        hosts_.list(filter, 0, proto::router::kMaxHostPageSize, &list);
        ASSERT_EQ(list.error_code(), proto::router::kErrorOk) << test_case.address;
        ASSERT_EQ(list.host_size(), test_case.count) << test_case.address;

        if (test_case.count == 1)
        {
            const HostId expected = std::string_view(test_case.address).starts_with("192") ? plain : special;
            EXPECT_EQ(list.host(0).host_id(), expected) << test_case.address;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// The display name is typed by the user in any letter case, the Cyrillic one included.
TEST_F(HostStoreTest, HostFilterMatchesTheDisplayNameInAnyCase)
{
    const HostId latin = addHost("hash-1");
    const HostId cyrillic = addHost("hash-2");
    ASSERT_NE(latin, kInvalidHostId);
    ASSERT_NE(cyrillic, kInvalidHostId);

    // A capitalized Cyrillic name and a lowercase part of it.
    const char kCyrillicName[] = "\xd0\xa1\xd0\xb5\xd1\x80\xd0\xb2\xd0\xb5\xd1\x80 1";
    const char kCyrillicPart[] = "\xd1\x81\xd0\xb5\xd1\x80\xd0\xb2";

    ASSERT_EQ(hosts_.modify(latin, findHost(latin).revision(), 0, 0, "Office PC", std::string_view()),
              proto::router::kErrorOk);
    ASSERT_EQ(hosts_.modify(cyrillic, findHost(cyrillic).revision(), 0, 0, kCyrillicName, std::string_view()),
              proto::router::kErrorOk);

    const struct { const char* name; HostId host_id; } kCases[] = {
        { "OFFICE", latin },
        { "ce p", latin },
        { kCyrillicPart, cyrillic }
    };

    for (const auto& test_case : kCases)
    {
        proto::router::HostFilter filter;
        filter.set_display_name(test_case.name);

        qint64 count = 0;
        ASSERT_EQ(hosts_.count(filter, &count), proto::router::kErrorOk) << test_case.name;
        EXPECT_EQ(count, 1) << test_case.name;

        proto::router::HostList list;
        hosts_.list(filter, 0, proto::router::kMaxHostPageSize, &list);
        ASSERT_EQ(list.error_code(), proto::router::kErrorOk) << test_case.name;
        ASSERT_EQ(list.host_size(), 1) << test_case.name;
        EXPECT_EQ(list.host(0).host_id(), test_case.host_id) << test_case.name;
    }
}

//--------------------------------------------------------------------------------------------------
// The ID is matched as a part of its decimal form, like the host search does it.
TEST_F(HostStoreTest, HostFilterMatchesAPartOfTheId)
{
    QList<HostId> host_ids;
    for (int i = 0; i < 12; ++i)
    {
        const HostId host_id = addHost("hash-" + std::to_string(i));
        ASSERT_NE(host_id, kInvalidHostId);
        host_ids.append(host_id);
    }

    for (const std::string& text : { std::to_string(host_ids.first()), std::to_string(host_ids.last()) })
    {
        QList<HostId> matches;
        for (HostId host_id : std::as_const(host_ids))
        {
            if (std::to_string(host_id).find(text) != std::string::npos)
                matches.append(host_id);
        }

        proto::router::HostFilter filter;
        filter.set_host_id(text);

        qint64 count = 0;
        ASSERT_EQ(hosts_.count(filter, &count), proto::router::kErrorOk) << text;
        EXPECT_EQ(count, matches.size()) << text;

        proto::router::HostList list;
        hosts_.list(filter, 0, proto::router::kMaxHostPageSize, &list);
        ASSERT_EQ(list.error_code(), proto::router::kErrorOk) << text;

        QList<HostId> found;
        for (const proto::router::Host& host : list.host())
            found.append(host.host_id());
        EXPECT_EQ(found, matches) << text;
    }
}

//--------------------------------------------------------------------------------------------------
// A host edit is applied only when it was built on the current state of the host. Two operators
// claim the same free host: the second claim was built on the state the first one already changed,
// so it loses and is told to refetch instead of silently stealing the host.
TEST_F(HostStoreTest, StaleHostEditIsRefused)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);
    const qint64 other_id = addWorkspace("beta");
    ASSERT_GT(other_id, 0);

    const qint64 seen_revision = findHost(host_id).revision();

    ASSERT_EQ(hosts_.modify(host_id, seen_revision, workspace_id, 0, "display",
                            std::string_view()),
              proto::router::kErrorOk);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);

    EXPECT_EQ(hosts_.modify(host_id, seen_revision, other_id, 0, "display", std::string_view()),
              proto::router::kErrorConflict);
    EXPECT_EQ(findHost(host_id).workspace_id(), workspace_id);

    // A workspace that is gone reads the same way: the sender acted on a stale snapshot.
    EXPECT_EQ(moveHost(host_id, 12345), proto::router::kErrorConflict);
    EXPECT_EQ(hosts_.modify(HostId(54321), 1, workspace_id, 0, "display", std::string_view()),
              proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// The race that must never lose data silently: one operator releases the host, another saves an
// edit built while the host was still in the workspace. The stale save must not claim the host
// back into the workspace it was just taken from.
TEST_F(HostStoreTest, ReleasedHostIsNotClaimedBackByAStaleEdit)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    ASSERT_EQ(moveHost(host_id, workspace_id), proto::router::kErrorOk);
    const proto::router::Host snapshot = findHost(host_id);

    ASSERT_EQ(moveHost(host_id, 0), proto::router::kErrorOk);

    EXPECT_EQ(hosts_.modify(host_id, snapshot.revision(), snapshot.workspace_id(),
                            snapshot.group_id(), "renamed", "note"),
              proto::router::kErrorConflict);

    const proto::router::Host after = findHost(host_id);
    EXPECT_EQ(after.workspace_id(), 0);
    EXPECT_TRUE(after.comment().empty());
}

//--------------------------------------------------------------------------------------------------
// The cascades that touch the operator-edited fields move the revision too: an edit built before
// the workspace (or the group) of the host was removed is as stale as one built before a direct
// edit.
TEST_F(HostStoreTest, CascadesMoveTheHostRevision)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(groups_.add(workspace_id, 0, "group", std::string_view(), &group_id), proto::router::kErrorOk);
    ASSERT_EQ(moveHost(host_id, workspace_id, group_id), proto::router::kErrorOk);

    // The group of the host goes away: the host drops to the workspace root and the edit built
    // while it was still in the group is stale.
    proto::router::Host snapshot = findHost(host_id);
    ASSERT_EQ(groups_.remove(workspace_id, group_id), proto::router::kErrorOk);
    EXPECT_GT(findHost(host_id).revision(), snapshot.revision());
    EXPECT_EQ(hosts_.modify(host_id, snapshot.revision(), workspace_id, 0, "renamed",
                            std::string_view()),
              proto::router::kErrorConflict);

    // The workspace goes away: the host is released and the pre-removal edit must not claim it
    // back.
    snapshot = findHost(host_id);
    ASSERT_EQ(workspaces_.remove(workspace_id), proto::router::kErrorOk);
    EXPECT_GT(findHost(host_id).revision(), snapshot.revision());
    EXPECT_EQ(findHost(host_id).workspace_id(), 0);
}

//--------------------------------------------------------------------------------------------------
// What the host reports about itself on connect does not move the revision: a reconnecting host
// must not fail the edit dialog an operator has open.
TEST_F(HostStoreTest, HostConnectDoesNotMoveTheRevision)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 seen_revision = findHost(host_id).revision();

    ASSERT_TRUE(hosts_.updateInfo(host_id, "hwid-1", "RENAMED-BY-OS", "x86_64", "3.0.1", "Windows", "192.168.1.11"));

    EXPECT_EQ(findHost(host_id).revision(), seen_revision);
}

//--------------------------------------------------------------------------------------------------
// The entry of a host is what tells a host that is only turned off from one the router no longer
// has: the client keeps credentials for a host and drops them by that answer.
TEST_F(HostStoreTest, HostEntryIsFoundWhileTheHostIsInTheDatabase)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    EXPECT_EQ(hosts_.checkEntry(host_id), proto::router::kErrorOk);
    EXPECT_EQ(hosts_.checkEntry(host_id + 1000), proto::router::kErrorNotFound);
    EXPECT_EQ(hosts_.checkEntry(kInvalidHostId), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// A read that failed says nothing about the host being gone. Collapsed into "not found", the
// answer would make every client that asks drop the credentials its user saved.
TEST_F(HostStoreTest, FailedHostEntryReadIsNotAMissingHost)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_TRUE(execRaw("DROP TABLE hosts"));

    EXPECT_EQ(hosts_.checkEntry(host_id), proto::router::kErrorInternalError);
}

//--------------------------------------------------------------------------------------------------
// Removal takes the entry out at once, before the host acknowledges the command, so from that
// moment the host reads as one the router does not have.
TEST_F(HostStoreTest, ScheduledRemovalTakesTheHostEntryOut)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_TRUE(hosts_.scheduleRemoval(host_id));

    EXPECT_EQ(hosts_.checkEntry(host_id), proto::router::kErrorNotFound);
    EXPECT_TRUE(hosts_.hasPendingRemoval(host_id));
}

//--------------------------------------------------------------------------------------------------
// A released host cannot keep the place it had inside the workspace it left; the note is its own
// and stays.
TEST_F(HostStoreTest, ReleasedHostLosesItsGroupAndKeepsItsComment)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    const qint64 workspace_id = addWorkspace("alpha");
    ASSERT_GT(workspace_id, 0);

    qint64 group_id = -1;
    ASSERT_EQ(groups_.add(workspace_id, 0, "group", std::string_view(), &group_id), proto::router::kErrorOk);

    ASSERT_EQ(hosts_.modify(host_id, findHost(host_id).revision(), workspace_id, group_id,
                            "display", "comment"),
              proto::router::kErrorOk);
    ASSERT_FALSE(findHost(host_id).comment().empty());

    ASSERT_EQ(moveHost(host_id, 0), proto::router::kErrorOk);

    const proto::router::Host host = findHost(host_id);
    EXPECT_EQ(host.workspace_id(), 0);
    EXPECT_EQ(host.group_id(), 0);
    EXPECT_EQ(host.comment(), "comment");
    EXPECT_EQ(host.display_name(), "display");
}

//--------------------------------------------------------------------------------------------------
// A host outside every workspace is edited like any other: its name and note are stored.
TEST_F(HostStoreTest, UnassignedHostKeepsItsComment)
{
    const HostId host_id = addHost("hash-1");
    ASSERT_NE(host_id, kInvalidHostId);

    ASSERT_EQ(hosts_.modify(host_id, findHost(host_id).revision(), 0, 0, "display", "note"), proto::router::kErrorOk);

    const proto::router::Host host = findHost(host_id);
    EXPECT_EQ(host.workspace_id(), 0);
    EXPECT_EQ(host.comment(), "note");
    EXPECT_EQ(host.display_name(), "display");
}

//--------------------------------------------------------------------------------------------------
// The very first AUTOINCREMENT insert into the database happens before sqlite_sequence exists;
// approving a host on such a database must succeed with the first permanent id.
TEST(HostStoreFreshTest, AddHostBeforeAnyAutoincrementInsert)
{
    QTemporaryDir temp_dir;
    ASSERT_TRUE(temp_dir.isValid());

    Database db;
    ASSERT_TRUE(db.open(temp_dir.path() + "/router.db3"));

    HostStore store(db);
    ASSERT_TRUE(store.add("hash-1", "hwid-1"));

    HostId host_id = kInvalidHostId;
    ASSERT_EQ(store.findByKey("hash-1", &host_id), proto::router::kErrorOk);
    EXPECT_EQ(host_id, HostId(1));
}
