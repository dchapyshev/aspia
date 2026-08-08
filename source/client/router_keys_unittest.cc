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

#include "client/router_test_fixture.h"

class RouterKeysTest : public RouterKeysFixture
{
protected:
    RouterKeys keys_;
};

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, UserKeysLoadTheIdentity)
{
    ASSERT_EQ(keys_.apply(userKeys(), SecureString(kPassword)), RouterKeys::Result::OK);

    EXPECT_EQ(keys_.userId(), kUserId);
    EXPECT_EQ(keys_.userName(), QString::fromUtf8(kUserName));
    EXPECT_TRUE(keys_.hasPrivateKey());
}

//--------------------------------------------------------------------------------------------------
// A record from before the key pair existed: the user must change its password before it holds
// a private key at all.
TEST_F(RouterKeysTest, UserKeysWithoutWrappedPrivateKeyAskForPasswordChange)
{
    proto::router::UserKeys keys = userKeys();
    keys.clear_wrap_private_key();

    EXPECT_EQ(keys_.apply(keys, SecureString(kPassword)),
              RouterKeys::Result::PASSWORD_CHANGE_REQUIRED);
    EXPECT_FALSE(keys_.hasPrivateKey());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, UserKeysWithWrongPasswordDoNotOpen)
{
    EXPECT_EQ(keys_.apply(userKeys(), SecureString("wrong-password")),
              RouterKeys::Result::DECRYPT_FAILED);
    EXPECT_FALSE(keys_.hasPrivateKey());
}

//--------------------------------------------------------------------------------------------------
TEST_F(RouterKeysTest, ClearDropsTheIdentity)
{
    loadKeys(&keys_);

    keys_.clear();

    EXPECT_EQ(keys_.userId(), 0);
    EXPECT_TRUE(keys_.userName().isEmpty());
    EXPECT_FALSE(keys_.hasPrivateKey());
}
