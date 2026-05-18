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

#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_string.h"
#include "proto/router_admin.h"

// ============================================================================
// create / isValid
// ============================================================================

TEST(router_user_test, create_generates_keypair)
{
    RouterUser user = RouterUser::create("testuser", SecureString("password123"));
    ASSERT_TRUE(user.isValid());

    EXPECT_EQ(user.public_key.size(), 32);
    EXPECT_FALSE(user.wrap_private_key.isEmpty());
    EXPECT_EQ(user.wrap_salt.size(), PrivateKeyCryptor::kSaltSize);
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

TEST(router_user_test, create_two_users_different_keypairs)
{
    RouterUser u1 = RouterUser::create("user1", SecureString("password"));
    RouterUser u2 = RouterUser::create("user2", SecureString("password"));
    ASSERT_TRUE(u1.isValid());
    ASSERT_TRUE(u2.isValid());

    EXPECT_NE(u1.public_key, u2.public_key);
    EXPECT_NE(u1.wrap_private_key, u2.wrap_private_key);
    EXPECT_NE(u1.wrap_salt, u2.wrap_salt);
}

TEST(router_user_test, default_router_user_is_invalid)
{
    RouterUser user;
    EXPECT_FALSE(user.isValid());
}

TEST(router_user_test, is_valid_requires_all_fields)
{
    RouterUser user = RouterUser::create("testuser", SecureString("password"));
    ASSERT_TRUE(user.isValid());

    RouterUser u_no_pk = user;
    u_no_pk.public_key.clear();
    EXPECT_FALSE(u_no_pk.isValid());

    RouterUser u_no_wsk = user;
    u_no_wsk.wrap_private_key.clear();
    EXPECT_FALSE(u_no_wsk.isValid());

    RouterUser u_no_salt = user;
    u_no_salt.wrap_salt.clear();
    EXPECT_FALSE(u_no_salt.isValid());

    // Base User validity still required.
    RouterUser u_no_name = user;
    u_no_name.name.clear();
    EXPECT_FALSE(u_no_name.isValid());
}

// ============================================================================
// Private key encryption roundtrip
// ============================================================================

TEST(router_user_test, wrap_private_key_decryptable_with_password)
{
    const SecureString password(QStringLiteral("Str0ngPass"));
    RouterUser user = RouterUser::create("alice", password);
    ASSERT_TRUE(user.isValid());

    SecureByteArray sk = PrivateKeyCryptor::decrypt(
        user.wrap_private_key, password, user.wrap_salt);
    ASSERT_FALSE(sk.isEmpty());
    EXPECT_EQ(sk.size(), 32);
}

TEST(router_user_test, wrap_private_key_not_decryptable_with_wrong_password)
{
    RouterUser user = RouterUser::create("alice", SecureString("correct"));
    ASSERT_TRUE(user.isValid());

    SecureByteArray sk = PrivateKeyCryptor::decrypt(
        user.wrap_private_key, SecureString("incorrect"), user.wrap_salt);
    EXPECT_TRUE(sk.isEmpty());
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
    EXPECT_EQ(restored.public_key, original.public_key);
    EXPECT_EQ(restored.wrap_private_key, original.wrap_private_key);
    EXPECT_EQ(restored.wrap_salt, original.wrap_salt);
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
    EXPECT_TRUE(proto_user.public_key().empty());
    EXPECT_TRUE(proto_user.wrap_private_key().empty());
    EXPECT_TRUE(proto_user.wrap_salt().empty());
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
    proto_user.set_public_key("pk_data");
    proto_user.set_wrap_private_key("wpk_data");
    proto_user.set_wrap_salt("ws_data");

    RouterUser user = RouterUser::parseFrom(proto_user);

    EXPECT_EQ(user.entry_id, 99);
    EXPECT_EQ(user.name, "bob");
    EXPECT_EQ(user.group, "4096");
    EXPECT_EQ(user.salt, QByteArray("salt_data"));
    EXPECT_EQ(user.verifier, QByteArray("verifier_data"));
    EXPECT_EQ(user.sessions, 3u);
    EXPECT_EQ(user.flags, static_cast<quint32>(User::ENABLED));
    EXPECT_EQ(user.public_key, QByteArray("pk_data"));
    EXPECT_EQ(user.wrap_private_key, QByteArray("wpk_data"));
    EXPECT_EQ(user.wrap_salt, QByteArray("ws_data"));
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
