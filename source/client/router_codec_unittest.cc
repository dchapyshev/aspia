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

#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "client/router_test_fixture.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

class RouterCodecTest : public RouterKeysFixture
{
protected:
    RouterKeys keys_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterCodecTest, HostFieldsAreDecryptedWithTheWorkspaceKey)
{
    loadKeys(&keys_, {10});

    proto::router::HostList list = hostList(10, {HostId(1)}, 1);
    list.mutable_host(0)->set_comment(encrypt(10, "comment"));
    list.mutable_host(0)->set_user_name(encrypt(10, "user"));
    list.mutable_host(0)->set_password(encrypt(10, "password"));

    const RouterHostList decoded = decodeRouterHostList(keys_, list);

    ASSERT_EQ(decoded.hosts.size(), 1);
    EXPECT_EQ(decoded.hosts.at(0).comment, "comment");
    EXPECT_EQ(decoded.hosts.at(0).user_name, "user");
    EXPECT_EQ(decoded.hosts.at(0).password.toString(), "password");
}

//--------------------------------------------------------------------------------------------------
// Without the key the encrypted fields stay empty; the plain ones still arrive, so the host is
// listed instead of disappearing.
TEST_F(RouterCodecTest, HostOfUnknownWorkspaceKeepsPlainFieldsOnly)
{
    loadKeys(&keys_, {10});

    proto::router::HostList list = hostList(20, {HostId(1)}, 1);
    list.mutable_host(0)->set_comment(encrypt(20, "comment"));

    const RouterHostList decoded = decodeRouterHostList(keys_, list);

    ASSERT_EQ(decoded.hosts.size(), 1);
    EXPECT_EQ(decoded.hosts.at(0).display_name, "host");
    EXPECT_TRUE(decoded.hosts.at(0).comment.isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The key travels only for a user that is being granted access; for the others the router keeps
// the entry it already stores.
TEST_F(RouterCodecTest, WorkspaceSaveSealsForNewMembersOnly)
{
    loadKeys(&keys_, {10});

    const RouterUser other = RouterUser::create("other", SecureString(kPassword));

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = "alpha";
    workspace.comment = "comment";
    workspace.revision = 3;
    workspace.access.append({ kUserId, QByteArray() });          // Already a member.
    workspace.access.append({ 2, other.public_key });            // Newly granted.
    workspace.host_ids.append(HostId(7));

    proto::router::Workspace out;
    ASSERT_EQ(buildRouterWorkspace(keys_, workspace, &out), proto::router::kErrorOk);

    EXPECT_EQ(out.entry_id(), 10);
    EXPECT_EQ(out.revision(), 3);
    ASSERT_EQ(out.access_size(), 2);
    EXPECT_TRUE(out.access(0).wrapped_gk().empty());
    EXPECT_TRUE(out.access(0).public_key().empty());
    EXPECT_FALSE(out.access(1).wrapped_gk().empty());
    EXPECT_EQ(out.access(1).public_key(), other.public_key.toStdString());
    ASSERT_EQ(out.host_id_size(), 1);
    EXPECT_EQ(out.host_id(0), 7u);

    // The new member can open exactly the key of this workspace.
    const SecureByteArray private_key = PrivateKeyCryptor::decrypt(
        other.wrap_private_key, SecureString(kPassword), other.wrap_salt);
    ASSERT_FALSE(private_key.isEmpty());

    const std::optional<SecureByteArray> opened = SealedBox::open(
        QByteArray::fromStdString(out.access(1).wrapped_gk()),
        KeyPair::fromPrivateKey(private_key));
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, groupKey(10));
}

//--------------------------------------------------------------------------------------------------
// Without the key of an existing workspace the save would encrypt its comment with a key nobody
// has, and the entries of the new members would be sealed to it as well.
TEST_F(RouterCodecTest, WorkspaceSaveWithoutItsKeyIsRefused)
{
    loadKeys(&keys_, {10});

    RouterWorkspace workspace;
    workspace.entry_id = 20;
    workspace.name = "beta";

    proto::router::Workspace out;
    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &out), proto::router::kErrorInternalError);
}

//--------------------------------------------------------------------------------------------------
// A workspace being created has no id yet, so its key is generated here - and must not be stored
// under the id 0, or the next created workspace would silently reuse it.
TEST_F(RouterCodecTest, NewWorkspaceGetsAFreshKeyThatIsNotKept)
{
    loadKeys(&keys_, {});

    RouterWorkspace workspace;
    workspace.name = "alpha";
    workspace.comment = "comment";

    proto::router::Workspace first;
    ASSERT_EQ(buildRouterWorkspace(keys_, workspace, &first), proto::router::kErrorOk);
    EXPECT_FALSE(keys_.hasWorkspaceKey(0));

    proto::router::Workspace second;
    ASSERT_EQ(buildRouterWorkspace(keys_, workspace, &second), proto::router::kErrorOk);

    // Two creations of the same workspace data do not produce the same ciphertext: the keys differ.
    EXPECT_NE(first.comment(), second.comment());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterCodecTest, HostAndGroupSavesRequireTheWorkspaceKey)
{
    loadKeys(&keys_, {10});

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 20;
    host.comment = "comment";

    proto::router::Host host_out;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInternalError);

    host.workspace_id = 10;
    ASSERT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorOk);
    EXPECT_FALSE(host_out.comment().empty());

    RouterGroup group;
    group.name = "servers";
    group.comment = "comment";

    proto::router::Group group_out;
    EXPECT_EQ(buildRouterGroup(keys_, 20, group, &group_out), proto::router::kErrorInternalError);
    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// The sender holds to the protocol bounds too. They count UTF-8 bytes, so a name of 64 non-ASCII
// characters is over the bound while the input field that accepted it is not.
TEST_F(RouterCodecTest, OversizedFieldsAreRefusedBeforeTheRequestIsSent)
{
    loadKeys(&keys_, {10});

    const QString long_ascii_name(proto::router::kMaxEntryNameLength + 1, QChar('n'));
    const QString cyrillic_name(proto::router::kMaxEntryNameLength / 2 + 1, QChar(0x0410));
    const QString long_comment(proto::router::kMaxCommentLength, QChar('c'));
    const QString long_credential(proto::router::kMaxCredentialLength, QChar('u'));

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;

    proto::router::Host host_out;

    host.display_name = long_ascii_name;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInvalidData);

    host.display_name = cyrillic_name;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInvalidData);

    host.display_name = "host";
    host.comment = long_comment;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInvalidData);

    host.comment.clear();
    host.user_name = long_credential;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInvalidData);

    host.user_name.clear();
    host.password = SecureString(long_credential);
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorInvalidData);

    RouterGroup group;
    group.name = long_ascii_name;

    proto::router::Group group_out;
    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorInvalidData);

    group.name = "servers";
    group.comment = long_comment;
    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorInvalidData);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = long_ascii_name;

    proto::router::Workspace workspace_out;
    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "alpha";
    workspace.comment = long_comment;
    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out),
              proto::router::kErrorInvalidData);
}

//--------------------------------------------------------------------------------------------------
// A group and a workspace must be named, and the router judges the name after trimming it. So does
// the sender, otherwise a name of blanks would pass here and be refused there.
TEST_F(RouterCodecTest, AnEmptyNameIsRefusedAndTrailingBlanksAreNot)
{
    loadKeys(&keys_, {10});

    RouterGroup group;
    proto::router::Group group_out;

    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorInvalidData);

    group.name = "   ";
    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorInvalidData);

    group.name = QString(proto::router::kMaxEntryNameLength, QChar('n')) + "   ";
    ASSERT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorOk);
    EXPECT_EQ(group_out.name().size(), proto::router::kMaxEntryNameLength);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    proto::router::Workspace workspace_out;

    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "   ";
    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out),
              proto::router::kErrorInvalidData);

    workspace.name = "  alpha  ";
    ASSERT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out), proto::router::kErrorOk);
    EXPECT_EQ(workspace_out.name(), "alpha");
}

//--------------------------------------------------------------------------------------------------
// A host is nameless until an administrator names it, and the display of it falls back to the
// computer name, so nothing about it is mandatory.
TEST_F(RouterCodecTest, AnEmptyHostIsSent)
{
    loadKeys(&keys_, {10});

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;

    proto::router::Host host_out;
    EXPECT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
// A record sitting exactly on the bounds goes out.
TEST_F(RouterCodecTest, FieldsAtTheBoundsAreSent)
{
    loadKeys(&keys_, {10});

    const QString name(proto::router::kMaxEntryNameLength, QChar('n'));

    RouterHost host;
    host.host_id = HostId(1);
    host.workspace_id = 10;
    host.display_name = name;

    proto::router::Host host_out;
    ASSERT_EQ(buildRouterHost(keys_, host, &host_out), proto::router::kErrorOk);
    EXPECT_EQ(host_out.display_name().size(), proto::router::kMaxEntryNameLength);

    RouterGroup group;
    group.name = name;

    proto::router::Group group_out;
    EXPECT_EQ(buildRouterGroup(keys_, 10, group, &group_out), proto::router::kErrorOk);

    RouterWorkspace workspace;
    workspace.entry_id = 10;
    workspace.name = name;

    proto::router::Workspace workspace_out;
    EXPECT_EQ(buildRouterWorkspace(keys_, workspace, &workspace_out), proto::router::kErrorOk);
}
