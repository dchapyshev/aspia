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

#include "base/peer/user.h"

#include <gtest/gtest.h>

namespace base {

// ============================================================================
// isValidUserName
// ============================================================================

TEST(user_test, valid_username_letters)
{
    EXPECT_TRUE(User::isValidUserName("admin"));
    EXPECT_TRUE(User::isValidUserName("Admin"));
    EXPECT_TRUE(User::isValidUserName("ADMIN"));
}

TEST(user_test, valid_username_with_digits)
{
    EXPECT_TRUE(User::isValidUserName("user123"));
    EXPECT_TRUE(User::isValidUserName("123"));
}

TEST(user_test, valid_username_special_chars)
{
    EXPECT_TRUE(User::isValidUserName("user.name"));
    EXPECT_TRUE(User::isValidUserName("user_name"));
    EXPECT_TRUE(User::isValidUserName("user-name"));
    EXPECT_TRUE(User::isValidUserName("user@domain"));
}

TEST(user_test, valid_username_single_char)
{
    EXPECT_TRUE(User::isValidUserName("a"));
    EXPECT_TRUE(User::isValidUserName("1"));
    EXPECT_TRUE(User::isValidUserName("_"));
}

TEST(user_test, invalid_username_empty)
{
    EXPECT_FALSE(User::isValidUserName(""));
}

TEST(user_test, invalid_username_too_long)
{
    QString long_name(User::kMaxUserNameLength + 1, 'a');
    EXPECT_FALSE(User::isValidUserName(long_name));
}

TEST(user_test, valid_username_max_length)
{
    QString max_name(User::kMaxUserNameLength, 'a');
    EXPECT_TRUE(User::isValidUserName(max_name));
}

TEST(user_test, invalid_username_forbidden_chars)
{
    EXPECT_FALSE(User::isValidUserName("user name"));  // space
    EXPECT_FALSE(User::isValidUserName("user!name"));  // exclamation
    EXPECT_FALSE(User::isValidUserName("user#name"));  // hash
    EXPECT_FALSE(User::isValidUserName("user$name"));  // dollar
    EXPECT_FALSE(User::isValidUserName("user/name"));  // slash
}

// ============================================================================
// isValidPassword
// ============================================================================

TEST(user_test, valid_password_min_length)
{
    QString password(User::kMinPasswordLength, 'x');
    EXPECT_TRUE(User::isValidPassword(password));
}

TEST(user_test, valid_password_max_length)
{
    QString password(User::kMaxPasswordLength, 'x');
    EXPECT_TRUE(User::isValidPassword(password));
}

TEST(user_test, invalid_password_empty)
{
    EXPECT_FALSE(User::isValidPassword(""));
}

TEST(user_test, invalid_password_too_long)
{
    QString password(User::kMaxPasswordLength + 1, 'x');
    EXPECT_FALSE(User::isValidPassword(password));
}

TEST(user_test, valid_password_normal)
{
    EXPECT_TRUE(User::isValidPassword("secret"));
    EXPECT_TRUE(User::isValidPassword("P@ssw0rd!"));
}

// ============================================================================
// isSafePassword
// ============================================================================

TEST(user_test, safe_password_meets_all_criteria)
{
    // >= kSafePasswordLength, has upper, lower, digit.
    EXPECT_TRUE(User::isSafePassword("Abcdefg1"));
    EXPECT_TRUE(User::isSafePassword("Password1"));
    EXPECT_TRUE(User::isSafePassword("12345Abc"));
}

TEST(user_test, unsafe_password_too_short)
{
    // Has upper, lower, digit but too short.
    EXPECT_FALSE(User::isSafePassword("Ab1"));
    EXPECT_FALSE(User::isSafePassword("Abc123"));
}

TEST(user_test, unsafe_password_no_upper)
{
    QString password(User::kSafePasswordLength, 'a');
    password[0] = '1'; // Has lower + digit, no upper.
    EXPECT_FALSE(User::isSafePassword(password));
}

TEST(user_test, unsafe_password_no_lower)
{
    QString password(User::kSafePasswordLength, 'A');
    password[0] = '1'; // Has upper + digit, no lower.
    EXPECT_FALSE(User::isSafePassword(password));
}

TEST(user_test, unsafe_password_no_digit)
{
    // Has upper + lower, no digit.
    EXPECT_FALSE(User::isSafePassword("Abcdefgh"));
}

TEST(user_test, safe_password_exact_min_length)
{
    // Exactly kSafePasswordLength characters with all criteria.
    QString password(User::kSafePasswordLength, 'a');
    password[0] = 'A';
    password[1] = '1';
    EXPECT_TRUE(User::isSafePassword(password));
}

// ============================================================================
// create / isValid
// ============================================================================

TEST(user_test, create_valid_user)
{
    User user = User::create("testuser", "password123");
    EXPECT_TRUE(user.isValid());
    EXPECT_EQ(user.name, "testuser");
    EXPECT_FALSE(user.salt.isEmpty());
    EXPECT_FALSE(user.verifier.isEmpty());
    EXPECT_FALSE(user.group.isEmpty());
}

TEST(user_test, create_with_empty_name)
{
    User user = User::create("", "password123");
    EXPECT_FALSE(user.isValid());
}

TEST(user_test, create_with_empty_password)
{
    User user = User::create("testuser", "");
    EXPECT_FALSE(user.isValid());
}

TEST(user_test, create_two_users_different_salts)
{
    User user1 = User::create("user1", "password");
    User user2 = User::create("user2", "password");

    EXPECT_TRUE(user1.isValid());
    EXPECT_TRUE(user2.isValid());

    // Random salt should differ between users.
    EXPECT_NE(user1.salt, user2.salt);
}

// ============================================================================
// isValid
// ============================================================================

TEST(user_test, default_user_is_invalid)
{
    User user;
    EXPECT_FALSE(user.isValid());
}

TEST(user_test, kInvalidUser_is_invalid)
{
    EXPECT_FALSE(User::kInvalidUser.isValid());
}

TEST(user_test, is_valid_requires_all_fields)
{
    User user = User::create("testuser", "password");
    ASSERT_TRUE(user.isValid());

    // Missing name.
    User u1 = user;
    u1.name.clear();
    EXPECT_FALSE(u1.isValid());

    // Missing salt.
    User u2 = user;
    u2.salt.clear();
    EXPECT_FALSE(u2.isValid());

    // Missing group.
    User u3 = user;
    u3.group.clear();
    EXPECT_FALSE(u3.isValid());

    // Missing verifier.
    User u4 = user;
    u4.verifier.clear();
    EXPECT_FALSE(u4.isValid());
}

// ============================================================================
// serialize / parseFrom roundtrip
// ============================================================================

TEST(user_test, serialize_parseFrom_roundtrip)
{
    User original = User::create("admin", "Str0ngPass");
    ASSERT_TRUE(original.isValid());

    original.entry_id = 42;
    original.sessions = 5;
    original.flags = User::ENABLED;

    proto::router::User proto_user = original.serialize();
    User restored = User::parseFrom(proto_user);

    EXPECT_EQ(restored.entry_id, original.entry_id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.group, original.group);
    EXPECT_EQ(restored.salt, original.salt);
    EXPECT_EQ(restored.verifier, original.verifier);
    EXPECT_EQ(restored.sessions, original.sessions);
    EXPECT_EQ(restored.flags, original.flags);
}

TEST(user_test, serialize_default_user)
{
    User user;
    proto::router::User proto_user = user.serialize();

    EXPECT_EQ(proto_user.entry_id(), 0);
    EXPECT_TRUE(proto_user.name().empty());
    EXPECT_TRUE(proto_user.group().empty());
    EXPECT_TRUE(proto_user.salt().empty());
    EXPECT_TRUE(proto_user.verifier().empty());
    EXPECT_EQ(proto_user.sessions(), 0u);
    EXPECT_EQ(proto_user.flags(), 0u);
}

TEST(user_test, parseFrom_preserves_all_fields)
{
    proto::router::User proto_user;
    proto_user.set_entry_id(99);
    proto_user.set_name("bob");
    proto_user.set_group("4096");
    proto_user.set_salt("salt_data");
    proto_user.set_verifier("verifier_data");
    proto_user.set_sessions(3);
    proto_user.set_flags(User::ENABLED);

    User user = User::parseFrom(proto_user);

    EXPECT_EQ(user.entry_id, 99);
    EXPECT_EQ(user.name, "bob");
    EXPECT_EQ(user.group, "4096");
    EXPECT_EQ(user.salt, QByteArray("salt_data"));
    EXPECT_EQ(user.verifier, QByteArray("verifier_data"));
    EXPECT_EQ(user.sessions, 3u);
    EXPECT_EQ(user.flags, static_cast<quint32>(User::ENABLED));
}

// ============================================================================
// Copy / move semantics
// ============================================================================

TEST(user_test, copy_constructor)
{
    User original = User::create("alice", "pass1234");
    ASSERT_TRUE(original.isValid());

    User copy(original);
    EXPECT_EQ(copy.name, original.name);
    EXPECT_EQ(copy.group, original.group);
    EXPECT_EQ(copy.salt, original.salt);
    EXPECT_EQ(copy.verifier, original.verifier);
}

TEST(user_test, copy_assignment)
{
    User original = User::create("alice", "pass1234");
    User copy;
    copy = original;
    EXPECT_EQ(copy.name, original.name);
    EXPECT_EQ(copy.salt, original.salt);
}

TEST(user_test, move_constructor)
{
    User original = User::create("alice", "pass1234");
    QString name = original.name;
    QByteArray salt = original.salt;

    User moved(std::move(original));
    EXPECT_EQ(moved.name, name);
    EXPECT_EQ(moved.salt, salt);
}

TEST(user_test, move_assignment)
{
    User original = User::create("alice", "pass1234");
    QString name = original.name;
    QByteArray salt = original.salt;

    User moved;
    moved = std::move(original);
    EXPECT_EQ(moved.name, name);
    EXPECT_EQ(moved.salt, salt);
}

// ============================================================================
// Flags
// ============================================================================

TEST(user_test, flags_enabled)
{
    User user = User::create("flaguser", "password");
    user.flags = User::ENABLED;
    EXPECT_EQ(user.flags & User::ENABLED, static_cast<quint32>(User::ENABLED));

    user.flags = 0;
    EXPECT_EQ(user.flags & User::ENABLED, 0u);
}

} // namespace base
