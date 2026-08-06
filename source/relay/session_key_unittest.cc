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

#include "relay/session_key.h"

#include <gtest/gtest.h>

#include "base/crypto/secure_byte_array.h"

namespace {

//--------------------------------------------------------------------------------------------------
std::string publicKeyOf(const SessionKey& session_key)
{
    return session_key.publicKey().toStdString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, CreatedKeyIsValid)
{
    SessionKey session_key = SessionKey::create();

    ASSERT_TRUE(session_key.isValid());
    EXPECT_FALSE(session_key.publicKey().isEmpty());
    EXPECT_FALSE(session_key.privateKey().isEmpty());

    // The initialization vector of the AEAD cipher.
    EXPECT_EQ(session_key.iv().size(), 12);
}

//--------------------------------------------------------------------------------------------------
// Only isValid() may be called on a key that was not created: the accessors go straight to the key
// pair, which has no OpenSSL object behind it yet.
TEST(SessionKeyTest, DefaultConstructedKeyIsInvalid)
{
    SessionKey session_key;

    EXPECT_FALSE(session_key.isValid());
    EXPECT_TRUE(session_key.iv().isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, EachKeyIsUnique)
{
    SessionKey first = SessionKey::create();
    SessionKey second = SessionKey::create();

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());

    EXPECT_NE(first.publicKey(), second.publicKey());
    EXPECT_NE(first.iv(), second.iv());
}

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, MoveTransfersKey)
{
    SessionKey source = SessionKey::create();
    ASSERT_TRUE(source.isValid());

    const QByteArray public_key = source.publicKey();
    const QByteArray iv = source.iv();

    SessionKey target(std::move(source));

    ASSERT_TRUE(target.isValid());
    EXPECT_EQ(target.publicKey(), public_key);
    EXPECT_EQ(target.iv(), iv);
}

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, SessionKeyIsDeterministic)
{
    SessionKey local = SessionKey::create();
    SessionKey peer = SessionKey::create();

    ASSERT_TRUE(local.isValid());
    ASSERT_TRUE(peer.isValid());

    const SecureByteArray first = local.sessionKey(publicKeyOf(peer));
    const SecureByteArray second = local.sessionKey(publicKeyOf(peer));

    EXPECT_FALSE(first.isEmpty());
    EXPECT_EQ(first, second);
}

//--------------------------------------------------------------------------------------------------
// The key is bound to both public keys, so the two peers of the same shared secret derive different
// keys. That is what makes the two directions of the relayed connection use different keys.
TEST(SessionKeyTest, SessionKeyIsBoundToBothPublicKeys)
{
    SessionKey first = SessionKey::create();
    SessionKey second = SessionKey::create();

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());

    EXPECT_NE(first.sessionKey(publicKeyOf(second)), second.sessionKey(publicKeyOf(first)));
}

//--------------------------------------------------------------------------------------------------
// The legacy variant hashes the bare shared secret, so both peers arrive at the same key.
TEST(SessionKeyTest, LegacySessionKeyIsSymmetric)
{
    SessionKey first = SessionKey::create();
    SessionKey second = SessionKey::create();

    ASSERT_TRUE(first.isValid());
    ASSERT_TRUE(second.isValid());

    const SecureByteArray first_key = first.legacySessionKey(publicKeyOf(second));
    const SecureByteArray second_key = second.legacySessionKey(publicKeyOf(first));

    EXPECT_FALSE(first_key.isEmpty());
    EXPECT_EQ(first_key, second_key);
}

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, DifferentPeersGetDifferentKeys)
{
    SessionKey local = SessionKey::create();
    SessionKey first_peer = SessionKey::create();
    SessionKey second_peer = SessionKey::create();

    ASSERT_TRUE(local.isValid());
    ASSERT_TRUE(first_peer.isValid());
    ASSERT_TRUE(second_peer.isValid());

    EXPECT_NE(local.sessionKey(publicKeyOf(first_peer)),
              local.sessionKey(publicKeyOf(second_peer)));
}

//--------------------------------------------------------------------------------------------------
TEST(SessionKeyTest, MalformedPeerKeyYieldsEmptySessionKey)
{
    SessionKey local = SessionKey::create();
    ASSERT_TRUE(local.isValid());

    EXPECT_TRUE(local.sessionKey(std::string()).isEmpty());
    EXPECT_TRUE(local.sessionKey(std::string("not a public key")).isEmpty());
    EXPECT_TRUE(local.legacySessionKey(std::string("not a public key")).isEmpty());
}
