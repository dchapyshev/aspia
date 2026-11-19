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

#include "client/config.h"

#include <gtest/gtest.h>

#include "base/build_config.h"
#include "proto/router.h"

namespace {

RouterConfig makeConfig()
{
    RouterConfig config;
    config.setAddress("router.example.com");
    config.setSessionType(proto::router::SESSION_TYPE_ADMIN);
    config.setUsername("user");
    config.setPassword(SecureString(QString("secret")));
    return config;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The account predicate decides whether the editors carry the synchronization over: the storage
// on the router belongs to the account, and its key stays the same when the password of the
// account changes. So the address and the user name make the account, and neither the password
// nor the session type is part of it. The address is the host and the port the device connects
// to, and the router compares the names of the users regardless of the case, so another spelling
// of the same host, port or name is the same account.
TEST(RouterConfigTest, SameAccountFollowsTheAddressAndTheUserName)
{
    const RouterConfig base = makeConfig();
    EXPECT_TRUE(base.hasSameAccount(makeConfig()));

    RouterConfig other = makeConfig();
    other.setAddress("moved.example.com");
    EXPECT_FALSE(base.hasSameAccount(other));

    other = makeConfig();
    other.setAddress("Router.Example.com");
    EXPECT_TRUE(base.hasSameAccount(other));

    other = makeConfig();
    other.setAddress(QString("router.example.com:%1").arg(kDefaultRouterClientTcpPort));
    EXPECT_TRUE(base.hasSameAccount(other));

    other = makeConfig();
    other.setAddress(QString("router.example.com:%1").arg(kDefaultRouterClientTcpPort + 1));
    EXPECT_FALSE(base.hasSameAccount(other));

    other = makeConfig();
    other.setUsername("somebody");
    EXPECT_FALSE(base.hasSameAccount(other));

    other = makeConfig();
    other.setUsername("User");
    EXPECT_TRUE(base.hasSameAccount(other));

    other = makeConfig();
    other.setPassword(SecureString(QString("rotated")));
    EXPECT_TRUE(base.hasSameAccount(other));

    other = makeConfig();
    other.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    EXPECT_TRUE(base.hasSameAccount(other));
}

//--------------------------------------------------------------------------------------------------
// Turning the synchronization on asks the user, unless it goes on with the same account or a known user.
TEST(RouterConfigTest, SyncConfirmationIsNeededUnlessTheAccountOrTheUserIsKnown)
{
    RouterConfig edited = makeConfig();
    edited.setSyncEnabled(true);

    RouterConfig another = edited;
    another.setUsername("somebody");

    RouterConfig stored = makeConfig();
    EXPECT_TRUE(stored.isSyncConfirmationNeeded(edited));
    EXPECT_TRUE(stored.isSyncConfirmationNeeded(another));

    stored.setSyncEnabled(true);
    EXPECT_FALSE(stored.isSyncConfirmationNeeded(edited));
    EXPECT_TRUE(stored.isSyncConfirmationNeeded(another));

    stored.setUserGuid("6f1c2b0e-5d4a-4c3b-9a8f-7e6d5c4b3a21");
    EXPECT_FALSE(stored.isSyncConfirmationNeeded(edited));
    EXPECT_FALSE(stored.isSyncConfirmationNeeded(another));

    stored.setSyncEnabled(false);
    EXPECT_TRUE(stored.isSyncConfirmationNeeded(another));

    another.setSyncEnabled(false);
    EXPECT_FALSE(stored.isSyncConfirmationNeeded(another));
}
