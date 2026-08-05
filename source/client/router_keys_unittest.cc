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

#include "client/router_keys.h"

#include <gtest/gtest.h>

#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "client/router_test_fixture.h"
#include "proto/router_admin.h"

class RouterKeysTest : public RouterKeysFixture
{
protected:
    RouterKeys keys_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, UserKeysLoadIdentityAndWorkspaceKeys)
{
    ASSERT_EQ(keys_.apply(userKeys({10, 20}), SecureString(kPassword)), RouterKeys::Result::OK);

    EXPECT_EQ(keys_.userId(), kUserId);
    EXPECT_EQ(keys_.userName(), QString::fromUtf8(kUserName));
    EXPECT_TRUE(keys_.hasPrivateKey());
    EXPECT_TRUE(keys_.hasWorkspaceKey(10));
    EXPECT_TRUE(keys_.hasWorkspaceKey(20));
    EXPECT_FALSE(keys_.hasWorkspaceKey(30));
    EXPECT_EQ(keys_.workspaceKeyCount(), 2);
}

//--------------------------------------------------------------------------------------------------
// A record from before the key pair existed: the user must change its password before it can hold
// any workspace key.
TEST_F(RouterKeysTest, UserKeysWithoutWrappedPrivateKeyAskForPasswordChange)
{
    proto::router::UserKeys keys = userKeys({10});
    keys.clear_wrap_private_key();

    EXPECT_EQ(keys_.apply(keys, SecureString(kPassword)),
              RouterKeys::Result::PASSWORD_CHANGE_REQUIRED);
    EXPECT_FALSE(keys_.hasPrivateKey());
    EXPECT_FALSE(keys_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, UserKeysWithWrongPasswordDoNotOpen)
{
    EXPECT_EQ(keys_.apply(userKeys({10}), SecureString("wrong-password")),
              RouterKeys::Result::DECRYPT_FAILED);
    EXPECT_FALSE(keys_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// A key sealed to somebody else is skipped instead of failing the whole session: the rest of the
// workspaces stay usable.
TEST_F(RouterKeysTest, UnopenableWorkspaceKeyIsSkipped)
{
    proto::router::UserKeys keys = userKeys({10});

    proto::router::UserKeys::WorkspaceKey* foreign = keys.add_workspace_key();
    foreign->set_workspace_id(20);
    foreign->set_wrapped_gk(SealedBox::seal(groupKey(20),
        RouterUser::create("other", SecureString(kPassword)).public_key)
            .toStdString());

    ASSERT_EQ(keys_.apply(keys, SecureString(kPassword)), RouterKeys::Result::OK);

    EXPECT_TRUE(keys_.hasWorkspaceKey(10));
    EXPECT_FALSE(keys_.hasWorkspaceKey(20));
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, ClearDropsIdentityAndKeys)
{
    loadKeys(&keys_, {10});

    keys_.clear();

    EXPECT_EQ(keys_.userId(), 0);
    EXPECT_FALSE(keys_.hasPrivateKey());
    EXPECT_FALSE(keys_.hasWorkspaceKey(10));
}

//--------------------------------------------------------------------------------------------------
// Creating an administrator and changing our own password both hand over every workspace key we
// hold, re-sealed to the key pair of the target.
TEST_F(RouterKeysTest, ResealCoversEveryHeldWorkspace)
{
    loadKeys(&keys_, {10, 20});

    const RouterUser other = RouterUser::create("other", SecureString(kPassword));

    proto::router::User user;
    keys_.resealGroupKeys(other.public_key, &user);

    ASSERT_EQ(user.workspace_key_size(), 2);

    const SecureByteArray private_key = PrivateKeyCryptor::decrypt(
        other.wrap_private_key, SecureString(kPassword), other.wrap_salt);
    ASSERT_FALSE(private_key.isEmpty());

    for (int i = 0; i < user.workspace_key_size(); ++i)
    {
        const std::optional<SecureByteArray> opened = SealedBox::open(
            QByteArray::fromStdString(user.workspace_key(i).wrapped_gk()),
            KeyPair::fromPrivateKey(private_key));
        ASSERT_TRUE(opened.has_value());
        EXPECT_EQ(*opened, groupKey(user.workspace_key(i).workspace_id()));
    }

    // A workspace we lost access to is not offered to anybody any more.
    keys_.dropKeysExcept({10});

    proto::router::User after;
    keys_.resealGroupKeys(other.public_key, &after);
    ASSERT_EQ(after.workspace_key_size(), 1);
    EXPECT_EQ(after.workspace_key(0).workspace_id(), 10);
}
