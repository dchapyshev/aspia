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

#include "client/router_codec.h"

#include <gtest/gtest.h>

#include "client/router_test_fixture.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

class RouterCodecTest : public RouterKeysFixture
{
};

//--------------------------------------------------------------------------------------------------
// What the router stores travels back as it is: the decoded record carries every field of the
// message.
TEST_F(RouterCodecTest, HostFieldsArriveAsStored)
{
    proto::router::HostList list = hostList(10, {HostId(1)}, 1);
    list.mutable_host(0)->set_comment("comment");

    const RouterHostList decoded = decodeRouterHostList(list);

    ASSERT_EQ(decoded.hosts.size(), 1);
    EXPECT_EQ(decoded.hosts.at(0).display_name, "host");
    EXPECT_EQ(decoded.hosts.at(0).comment, "comment");
    EXPECT_EQ(decoded.hosts.at(0).workspace_id, 10);
}

//--------------------------------------------------------------------------------------------------
// The save carries the membership and the host set of the workspace as the operator built them.
TEST_F(RouterCodecTest, WorkspaceSaveCarriesMembershipAndHosts)
{
    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = "alpha";
    workspace.comment = "comment";
    workspace.revision = 3;
    workspace.access.append({ kUserId });
    workspace.access.append({ 2 });
    workspace.host_ids.append(HostId(7));

    proto::router::Workspace out;
    ASSERT_EQ(buildRouterWorkspace(workspace, &out), proto::router::kErrorOk);

    EXPECT_EQ(out.entry_id(), 10);
    EXPECT_EQ(out.name(), "alpha");
    EXPECT_EQ(out.comment(), "comment");
    EXPECT_EQ(out.revision(), 3);
    ASSERT_EQ(out.access_size(), 2);
    EXPECT_EQ(out.access(0).user_id(), kUserId);
    EXPECT_EQ(out.access(1).user_id(), 2);
    ASSERT_EQ(out.host_id_size(), 1);
    EXPECT_EQ(out.host_id(0), 7u);
}

//--------------------------------------------------------------------------------------------------
// The sender holds to the protocol bounds too. They count UTF-8 bytes, so a name of 64 non-ASCII
// characters is over the bound while the input field that accepted it is not.
TEST_F(RouterCodecTest, OversizedFieldsAreRefusedBeforeTheRequestIsSent)
{
    const QString long_ascii_name(proto::router::kMaxEntryNameLength + 1, QChar('n'));
    const QString cyrillic_name(proto::router::kMaxEntryNameLength / 2 + 1, QChar(0x0410));
    const QString long_comment(proto::router::kMaxCommentLength + 1, QChar('c'));

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;

    proto::router::Host host_out;

    host.display_name = long_ascii_name;
    EXPECT_EQ(buildRouterHost(host, &host_out), proto::router::kErrorInvalidData);

    host.display_name = cyrillic_name;
    EXPECT_EQ(buildRouterHost(host, &host_out), proto::router::kErrorInvalidData);

    host.display_name = "host";
    host.comment = long_comment;
    EXPECT_EQ(buildRouterHost(host, &host_out), proto::router::kErrorInvalidData);

    RouterGroup group;
    group.name = long_ascii_name;

    proto::router::Group group_out;
    EXPECT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorInvalidData);

    group.name = "servers";
    group.comment = long_comment;
    EXPECT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorInvalidData);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = long_ascii_name;

    proto::router::Workspace workspace_out;
    EXPECT_EQ(buildRouterWorkspace(workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "alpha";
    workspace.comment = long_comment;
    EXPECT_EQ(buildRouterWorkspace(workspace, &workspace_out),
              proto::router::kErrorInvalidData);
}

//--------------------------------------------------------------------------------------------------
// A group and a workspace must be named, and the router judges the name after trimming it. So does
// the sender, otherwise a name of blanks would pass here and be refused there.
TEST_F(RouterCodecTest, AnEmptyNameIsRefusedAndTrailingBlanksAreNot)
{
    RouterGroup group;
    proto::router::Group group_out;

    EXPECT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorInvalidData);

    group.name = "   ";
    EXPECT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorInvalidData);

    group.name = QString(proto::router::kMaxEntryNameLength, QChar('n')) + "   ";
    ASSERT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorOk);
    EXPECT_EQ(group_out.name().size(), proto::router::kMaxEntryNameLength);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    proto::router::Workspace workspace_out;

    EXPECT_EQ(buildRouterWorkspace(workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "   ";
    EXPECT_EQ(buildRouterWorkspace(workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "  alpha  ";
    ASSERT_EQ(buildRouterWorkspace(workspace, &workspace_out), proto::router::kErrorOk);
    EXPECT_EQ(workspace_out.name(), "alpha");
}

//--------------------------------------------------------------------------------------------------
// A host is nameless until an administrator names it, and the display of it falls back to the
// computer name, so nothing about it is mandatory.
TEST_F(RouterCodecTest, AnEmptyHostIsSent)
{
    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;

    proto::router::Host host_out;
    EXPECT_EQ(buildRouterHost(host, &host_out), proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// A record sitting exactly on the bounds goes out.
TEST_F(RouterCodecTest, FieldsAtTheBoundsAreSent)
{
    const QString name(proto::router::kMaxEntryNameLength, QChar('n'));

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;
    host.display_name = name;

    proto::router::Host host_out;
    ASSERT_EQ(buildRouterHost(host, &host_out), proto::router::kErrorOk);
    EXPECT_EQ(host_out.display_name().size(), proto::router::kMaxEntryNameLength);

    RouterGroup group;
    group.name = name;

    proto::router::Group group_out;
    EXPECT_EQ(buildRouterGroup(group, &group_out), proto::router::kErrorOk);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = name;

    proto::router::Workspace workspace_out;
    EXPECT_EQ(buildRouterWorkspace(workspace, &workspace_out), proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The count of a scope is never negative. One that arrives so is not data the pagination can work
// with - it takes a non-negative count as its contract and ends the process on anything else - so
// the codec is where it stops.
TEST_F(RouterCodecTest, NegativeTotalCountDoesNotReachTheCallers)
{
    proto::router::HostList list = hostList(10, {HostId(1)}, -5);
    EXPECT_EQ(decodeRouterHostList(list).total_count, 0);

    proto::router::HostSearchResult search;
    search.set_error_code(proto::router::kErrorOk);
    search.set_total_count(-5);
    EXPECT_EQ(decodeRouterHostSearchResult(search).total_count, 0);
}
