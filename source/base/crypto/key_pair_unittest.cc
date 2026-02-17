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

#include "base/crypto/key_pair.h"

#include <gtest/gtest.h>

namespace base {

TEST(KeyPair, SessionKey)
{
    KeyPair side1 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(side1.isValid());
    EXPECT_TRUE(side1.privateKey() != side1.publicKey());

    KeyPair side2 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(side2.isValid());
    EXPECT_TRUE(side2.privateKey() != side2.publicKey());

    EXPECT_TRUE(side1.privateKey() != side2.privateKey());
    EXPECT_TRUE(side1.publicKey() != side2.publicKey());

    QByteArray session_key1 = side1.sessionKey(side2.publicKey());
    EXPECT_FALSE(session_key1.isEmpty());

    QByteArray session_key2 = side2.sessionKey(side1.publicKey());
    EXPECT_FALSE(session_key2.isEmpty());

    EXPECT_TRUE(session_key1 == session_key2);
}

TEST(KeyPair, Convert)
{
    KeyPair pair1 = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(pair1.isValid());
    EXPECT_TRUE(pair1.privateKey() != pair1.publicKey());

    QByteArray private_key1 = pair1.privateKey();
    EXPECT_FALSE(private_key1.isEmpty());

    QByteArray public_key1 = pair1.publicKey();
    EXPECT_FALSE(public_key1.isEmpty());

    KeyPair pair2 = KeyPair::fromPrivateKey(private_key1);
    EXPECT_TRUE(pair2.isValid());
    EXPECT_TRUE(pair2.privateKey() != pair2.publicKey());

    QByteArray private_key2 = pair2.privateKey();
    EXPECT_FALSE(private_key2.isEmpty());

    QByteArray public_key2 = pair2.publicKey();
    EXPECT_FALSE(public_key2.isEmpty());

    EXPECT_TRUE(private_key1 == private_key2);
    EXPECT_TRUE(public_key1 == public_key2);
}

TEST(KeyPair, Multiple)
{
    KeyPair pair = KeyPair::create(KeyPair::Type::X25519);
    EXPECT_TRUE(pair.isValid());

    QByteArray private_key = pair.privateKey();
    EXPECT_FALSE(private_key.isEmpty());

    QByteArray public_key = pair.publicKey();
    EXPECT_FALSE(public_key.isEmpty());

    EXPECT_TRUE(pair.privateKey() != pair.publicKey());

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_TRUE(private_key == pair.privateKey());
        EXPECT_TRUE(public_key == pair.publicKey());
    }
}

} // namespace base
