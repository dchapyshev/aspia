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

#include "base/peer/router_user.h"

#include <gtest/gtest.h>

#include "base/crypto/secure_string.h"
#include "proto/router_admin.h"

// ============================================================================
// create / isValid
// ============================================================================

TEST(router_user_test, create_produces_srp_material)
{
    RouterUser user = RouterUser::create("testuser", SecureString("password123"));
    ASSERT_TRUE(user.isValid());

    EXPECT_FALSE(user.salt.isEmpty());
    EXPECT_FALSE(user.verifier.isEmpty());
}

TEST(router_user_test, create_with_empty_name)
{
    RouterUser user = RouterUser::create("", SecureString("password"));
    EXPECT_FALSE(user.isValid());
}

TEST(router_user_test, create_with_empty_password)
{
    RouterUser user = RouterUser::create("testuser", SecureString(""));
    EXPECT_FALSE(user.isValid());
}

TEST(router_user_test, default_router_user_is_invalid)
{
    RouterUser user;
    EXPECT_FALSE(user.isValid());
}

// A record is valid only if it can actually be used. A name the authenticator would refuse and an
// SRP group nobody has the parameters for both make the account impossible to log into.
TEST(router_user_test, is_valid_rejects_an_unusable_name_or_group)
{
    RouterUser user = RouterUser::create("testuser", SecureString("password"));
    ASSERT_TRUE(user.isValid());

    RouterUser blank_name = user;
    blank_name.name = "   ";
    EXPECT_FALSE(blank_name.isValid());

    RouterUser bad_name = user;
    bad_name.name = "bob smith";
    EXPECT_FALSE(bad_name.isValid());

    RouterUser long_name = user;
    long_name.name = QString(User::kMaxUserNameLength + 1, QChar('n'));
    EXPECT_FALSE(long_name.isValid());

    RouterUser unknown_group = user;
    unknown_group.group = "1024";
    EXPECT_FALSE(unknown_group.isValid());
}

// With a verifier of 0 or 1 the session key follows from the exchange alone, so the account would
// let in anybody who knows the name.
TEST(router_user_test, is_valid_rejects_a_degenerate_verifier)
{
    RouterUser user = RouterUser::create("testuser", SecureString("password"));
    ASSERT_TRUE(user.isValid());

    RouterUser zero_verifier = user;
    zero_verifier.verifier = QByteArray(1, '\x00');
    EXPECT_FALSE(zero_verifier.isValid());

    RouterUser one_verifier = user;
    one_verifier.verifier = QByteArray(1, '\x01');
    EXPECT_FALSE(one_verifier.isValid());
}

// ============================================================================
// serialize / parseFrom roundtrip
// ============================================================================

TEST(router_user_test, serialize_parseFrom_roundtrip)
{
    RouterUser original = RouterUser::create("admin", SecureString("Str0ngPass"));
    ASSERT_TRUE(original.isValid());

    original.entry_id = 42;
    original.sessions = 5;
    original.flags = User::ENABLED;

    proto::router::User proto_user = original.serialize();
    RouterUser restored = RouterUser::parseFrom(proto_user);

    EXPECT_EQ(restored.entry_id, original.entry_id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.group, original.group);
    EXPECT_EQ(restored.salt, original.salt);
    EXPECT_EQ(restored.verifier, original.verifier);
    EXPECT_EQ(restored.sessions, original.sessions);
    EXPECT_EQ(restored.flags, original.flags);
}

TEST(router_user_test, serialize_default_router_user)
{
    RouterUser user;
    proto::router::User proto_user = user.serialize();

    EXPECT_EQ(proto_user.entry_id(), 0);
    EXPECT_TRUE(proto_user.name().empty());
    EXPECT_TRUE(proto_user.group().empty());
    EXPECT_TRUE(proto_user.salt().empty());
    EXPECT_TRUE(proto_user.verifier().empty());
    EXPECT_EQ(proto_user.sessions(), 0u);
    EXPECT_EQ(proto_user.flags(), 0u);
}

TEST(router_user_test, parseFrom_preserves_all_fields)
{
    proto::router::User proto_user;
    proto_user.set_entry_id(99);
    proto_user.set_name("bob");
    proto_user.set_group("4096");
    proto_user.set_salt("salt_data");
    proto_user.set_verifier("verifier_data");
    proto_user.set_sessions(3);
    proto_user.set_flags(User::ENABLED);

    RouterUser user = RouterUser::parseFrom(proto_user);

    EXPECT_EQ(user.entry_id, 99);
    EXPECT_EQ(user.name, "bob");
    EXPECT_EQ(user.group, "4096");
    EXPECT_EQ(user.salt, QByteArray("salt_data"));
    EXPECT_EQ(user.verifier, QByteArray("verifier_data"));
    EXPECT_EQ(user.sessions, 3u);
    EXPECT_EQ(user.flags, static_cast<quint32>(User::ENABLED));
}

// ============================================================================
// Slicing to base User
// ============================================================================

TEST(router_user_test, slicing_to_user_preserves_base_fields)
{
    RouterUser router_user = RouterUser::create("alice", SecureString("password"));
    ASSERT_TRUE(router_user.isValid());

    User base = router_user; // implicit slicing copy
    EXPECT_EQ(base.name, router_user.name);
    EXPECT_EQ(base.salt, router_user.salt);
    EXPECT_EQ(base.verifier, router_user.verifier);
    EXPECT_TRUE(base.isValid());
}
