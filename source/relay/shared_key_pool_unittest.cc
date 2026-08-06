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

#include "relay/shared_key_pool.h"

#include <gtest/gtest.h>

#include "relay/session_key.h"

// The pool is a process-wide singleton, so every test starts from an empty one.
class SharedKeyPoolTest : public testing::Test
{
protected:
    void SetUp() final { SharedKeyPool::instance().clear(); }
    void TearDown() final { SharedKeyPool::instance().clear(); }

    SharedKeyPool& pool() { return SharedKeyPool::instance(); }
};

//--------------------------------------------------------------------------------------------------
TEST_F(SharedKeyPoolTest, AddedKeyIsFoundById)
{
    SessionKey session_key = SessionKey::create();
    ASSERT_TRUE(session_key.isValid());

    const QByteArray public_key = session_key.publicKey();
    const QByteArray iv = session_key.iv();

    const quint32 key_id = pool().add(std::move(session_key));
    EXPECT_EQ(pool().count(), 1U);

    SessionKey peer = SessionKey::create();
    ASSERT_TRUE(peer.isValid());

    std::optional<SharedKeyPool::Key> key = pool().find(key_id, peer.publicKey().toStdString());
    ASSERT_TRUE(key.has_value());
    EXPECT_FALSE(key->first.isEmpty());
    EXPECT_EQ(key->second, iv);

    // The pool derives the key from the stored key pair, not from a copy of it.
    EXPECT_NE(public_key, peer.publicKey());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SharedKeyPoolTest, KeyIdsAreUnique)
{
    const quint32 first_id = pool().add(SessionKey::create());
    const quint32 second_id = pool().add(SessionKey::create());
    const quint32 third_id = pool().add(SessionKey::create());

    EXPECT_NE(first_id, second_id);
    EXPECT_NE(second_id, third_id);
    EXPECT_NE(first_id, third_id);
    EXPECT_EQ(pool().count(), 3U);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SharedKeyPoolTest, UnknownKeyIsNotFound)
{
    const quint32 key_id = pool().add(SessionKey::create());

    SessionKey peer = SessionKey::create();
    ASSERT_TRUE(peer.isValid());

    const std::string peer_public_key = peer.publicKey().toStdString();

    EXPECT_FALSE(pool().find(key_id + 1, peer_public_key).has_value());
    EXPECT_FALSE(pool().findLegacy(key_id + 1, peer_public_key).has_value());
}

//--------------------------------------------------------------------------------------------------
TEST_F(SharedKeyPoolTest, RemovedKeyIsGone)
{
    const quint32 key_id = pool().add(SessionKey::create());

    SessionKey peer = SessionKey::create();
    ASSERT_TRUE(peer.isValid());

    const std::string peer_public_key = peer.publicKey().toStdString();

    ASSERT_TRUE(pool().find(key_id, peer_public_key).has_value());

    EXPECT_TRUE(pool().remove(key_id));
    EXPECT_EQ(pool().count(), 0U);
    EXPECT_FALSE(pool().find(key_id, peer_public_key).has_value());

    // Removing the same key twice is not an error, but reports that there was nothing to remove.
    EXPECT_FALSE(pool().remove(key_id));
}

//--------------------------------------------------------------------------------------------------
// A key removed from the pool is gone even if its identifier is reused by a later addition: the
// counter never goes back.
TEST_F(SharedKeyPoolTest, KeyIdIsNotReused)
{
    const quint32 first_id = pool().add(SessionKey::create());
    ASSERT_TRUE(pool().remove(first_id));

    const quint32 second_id = pool().add(SessionKey::create());
    EXPECT_NE(first_id, second_id);
}

//--------------------------------------------------------------------------------------------------
// The two lookups differ only in the cipher the peer negotiated, so they must derive different keys
// but report the same initialization vector.
TEST_F(SharedKeyPoolTest, LegacyLookupDerivesAnotherKey)
{
    const quint32 key_id = pool().add(SessionKey::create());

    SessionKey peer = SessionKey::create();
    ASSERT_TRUE(peer.isValid());

    const std::string peer_public_key = peer.publicKey().toStdString();

    std::optional<SharedKeyPool::Key> key = pool().find(key_id, peer_public_key);
    std::optional<SharedKeyPool::Key> legacy_key = pool().findLegacy(key_id, peer_public_key);

    ASSERT_TRUE(key.has_value());
    ASSERT_TRUE(legacy_key.has_value());

    EXPECT_NE(key->first, legacy_key->first);
    EXPECT_EQ(key->second, legacy_key->second);
}

//--------------------------------------------------------------------------------------------------
TEST_F(SharedKeyPoolTest, ClearRemovesAllKeys)
{
    pool().add(SessionKey::create());
    pool().add(SessionKey::create());
    ASSERT_EQ(pool().count(), 2U);

    pool().clear();
    EXPECT_EQ(pool().count(), 0U);
}
