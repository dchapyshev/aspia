//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/crypto/key_pair.h"

#include <gtest/gtest.h>

namespace base {

TEST(KeyPair, SessionKey)
{
    KeyPair side1 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(side1.isValid());
    EXPECT_NE(compare(side1.privateKey(), side1.publicKey()), 0);

    KeyPair side2 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(side2.isValid());
    EXPECT_NE(compare(side2.privateKey(), side2.publicKey()), 0);

    EXPECT_NE(compare(side1.privateKey(), side2.privateKey()), 0);
    EXPECT_NE(compare(side1.publicKey(), side2.publicKey()), 0);

    ByteArray session_key1 = side1.sessionKey(side2.publicKey());
    EXPECT_FALSE(session_key1.empty());

    ByteArray session_key2 = side2.sessionKey(side1.publicKey());
    EXPECT_FALSE(session_key2.empty());

    EXPECT_EQ(compare(session_key1, session_key2), 0);
}

TEST(KeyPair, Convert)
{
    KeyPair pair1 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(pair1.isValid());
    EXPECT_NE(compare(pair1.privateKey(), pair1.publicKey()), 0);

    ByteArray private_key1 = pair1.privateKey();
    EXPECT_FALSE(private_key1.empty());

    ByteArray public_key1 = pair1.publicKey();
    EXPECT_FALSE(public_key1.empty());

    KeyPair pair2 = KeyPair::fromPrivateKey(private_key1);
    EXPECT_TRUE(pair2.isValid());
    EXPECT_NE(compare(pair2.privateKey(), pair2.publicKey()), 0);

    ByteArray private_key2 = pair2.privateKey();
    EXPECT_FALSE(private_key2.empty());

    ByteArray public_key2 = pair2.publicKey();
    EXPECT_FALSE(public_key2.empty());

    EXPECT_EQ(compare(private_key1, private_key2), 0);
    EXPECT_EQ(compare(public_key1, public_key2), 0);
}

TEST(KeyPair, Multiple)
{
    KeyPair pair = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(pair.isValid());

    ByteArray private_key = pair.privateKey();
    EXPECT_FALSE(private_key.empty());

    ByteArray public_key = pair.publicKey();
    EXPECT_FALSE(public_key.empty());

    EXPECT_NE(compare(pair.privateKey(), pair.publicKey()), 0);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(compare(private_key, pair.privateKey()), 0);
        EXPECT_EQ(compare(public_key, pair.publicKey()), 0);
    }
}

} // namespace base
