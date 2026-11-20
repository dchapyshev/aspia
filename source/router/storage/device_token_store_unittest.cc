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

#include "router/storage/device_token_store.h"

#include "router/router_test_base.h"

using DeviceTokenStoreTest = RouterTestBase;

//--------------------------------------------------------------------------------------------------
// The tokens of one user are bounded: a device that logs in anew every time cannot grow the list
// past the cap the protocol counts on. The least recently used ones go first.
TEST_F(DeviceTokenStoreTest, DeviceTokensAreCappedPerUser)
{
    std::string first_token;
    ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &first_token));

    for (int i = 0; i < proto::router::kMaxDeviceTokensPerUser; ++i)
    {
        std::string token;
        ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &token));
    }

    std::vector<DeviceToken> tokens;
    ASSERT_EQ(device_tokens_.list(admin_.entry_id, &tokens), proto::router::kErrorOk);
    EXPECT_EQ(tokens.size(), size_t(proto::router::kMaxDeviceTokensPerUser));

    qint64 user_id = 0;
    EXPECT_EQ(device_tokens_.find(first_token, &user_id), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// The token list tells a user with no devices from a user that is not there, the way the
// revocation does.
TEST_F(DeviceTokenStoreTest, TokenListOfAMissingUserIsNotFound)
{
    std::vector<DeviceToken> tokens;
    EXPECT_EQ(device_tokens_.list(admin_.entry_id, &tokens), proto::router::kErrorOk);
    EXPECT_TRUE(tokens.empty());

    EXPECT_EQ(device_tokens_.list(9999, &tokens), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// The cap evicts by the last use and not by the creation: the oldest token of the list survives
// the overflow when it was just presented, and the one nobody used for a day goes instead.
TEST_F(DeviceTokenStoreTest, TrimEvictsTheLeastRecentlyUsedToken)
{
    std::string first_token;
    ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &first_token));

    std::string second_token;
    qint64 second_id = 0;
    ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &second_token, &second_id));

    for (int i = 2; i < proto::router::kMaxDeviceTokensPerUser; ++i)
    {
        std::string token;
        ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &token));
    }

    ASSERT_TRUE(device_tokens_.touch(first_token, "127.0.0.1"));
    ASSERT_TRUE(execRaw(QString("UPDATE client_device_tokens SET last_used_at=last_used_at-86400 "
                                "WHERE token_id=%1").arg(second_id)));

    std::string overflow_token;
    ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &overflow_token));

    qint64 user_id = 0;
    EXPECT_EQ(device_tokens_.find(first_token, &user_id), proto::router::kErrorOk);
    EXPECT_EQ(device_tokens_.find(second_token, &user_id), proto::router::kErrorNotFound);
}

//--------------------------------------------------------------------------------------------------
// The touch answers whether it refreshed anything. A token can be revoked from another process
// between the lookup and the touch, and the caller logs by this answer.
TEST_F(DeviceTokenStoreTest, TouchOfAMissingTokenReportsFalse)
{
    std::string token;
    ASSERT_TRUE(device_tokens_.issue(admin_.entry_id, "127.0.0.1", &token));

    EXPECT_TRUE(device_tokens_.touch(token, "127.0.0.2"));

    ASSERT_EQ(device_tokens_.revokeAll(admin_.entry_id), proto::router::kErrorOk);

    EXPECT_FALSE(device_tokens_.touch(token, "127.0.0.2"));
}
