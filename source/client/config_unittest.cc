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

#include "proto/router.h"

namespace {

RouterConfig makeConfig()
{
    RouterConfig config;
    config.setAddress("router.example.com");
    config.setSessionType(proto::router::SESSION_TYPE_ADMIN);
    config.setUsername("user");
    config.setPassword(SecureString(QString("secret")));
    config.setDisplayName("display");
    return config;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The predicate decides whether a record still names the same account. The reload of the
// controller reconnects on a mismatch, and the editors carry the device token over only on a
// match. The display name is not part of the account, so a rename keeps both the connection and
// the token.
TEST(RouterConfigTest, SameParamsFollowTheAccountNotTheName)
{
    const RouterConfig base = makeConfig();
    EXPECT_TRUE(base.hasSameParams(makeConfig()));

    RouterConfig other = makeConfig();
    other.setAddress("moved.example.com");
    EXPECT_FALSE(base.hasSameParams(other));

    other = makeConfig();
    other.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    EXPECT_FALSE(base.hasSameParams(other));

    other = makeConfig();
    other.setUsername("somebody");
    EXPECT_FALSE(base.hasSameParams(other));

    other = makeConfig();
    other.setPassword(SecureString(QString("rotated")));
    EXPECT_FALSE(base.hasSameParams(other));

    other = makeConfig();
    other.setDisplayName("renamed");
    EXPECT_TRUE(base.hasSameParams(other));
}
