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

#include "base/peer/device_auth.h"

#include <gtest/gtest.h>

#include "base/crypto/random.h"

namespace {

QByteArray randomTokenId()
{
    return Random::byteArray(DeviceAuth::kTokenIdSize);
}

} // namespace

TEST(DeviceAuth, GenerateDeviceKeyHasExpectedSizes)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());
    EXPECT_EQ(key.private_key.size(), DeviceAuth::kPrivateKeySize);
    EXPECT_EQ(key.public_key.size(), DeviceAuth::kPublicKeySize);
}

TEST(DeviceAuth, DerivePublicMatchesGenerated)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    const QByteArray derived = DeviceAuth::derivePublicKey(key.private_key);
    EXPECT_EQ(derived, key.public_key);
}

TEST(DeviceAuth, RoundTripVerifies)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    const QByteArray token_id = randomTokenId();
    const QByteArray nonce = DeviceAuth::generateServerNonce();

    const QByteArray sig = DeviceAuth::signChallenge(key.private_key, token_id, nonce);
    ASSERT_EQ(sig.size(), DeviceAuth::kSignatureSize);

    EXPECT_TRUE(DeviceAuth::verifyChallenge(key.public_key, token_id, nonce, sig));
}

TEST(DeviceAuth, TamperedTokenIdRejected)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    QByteArray token_id = randomTokenId();
    const QByteArray nonce = DeviceAuth::generateServerNonce();
    const QByteArray sig = DeviceAuth::signChallenge(key.private_key, token_id, nonce);

    token_id[0] = token_id[0] ^ 0x01;
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key, token_id, nonce, sig));
}

TEST(DeviceAuth, TamperedNonceRejected)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    const QByteArray token_id = randomTokenId();
    QByteArray nonce = DeviceAuth::generateServerNonce();
    const QByteArray sig = DeviceAuth::signChallenge(key.private_key, token_id, nonce);

    nonce[0] = nonce[0] ^ 0x01;
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key, token_id, nonce, sig));
}

TEST(DeviceAuth, TamperedSignatureRejected)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    const QByteArray token_id = randomTokenId();
    const QByteArray nonce = DeviceAuth::generateServerNonce();
    QByteArray sig = DeviceAuth::signChallenge(key.private_key, token_id, nonce);

    sig[0] = sig[0] ^ 0x01;
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key, token_id, nonce, sig));
}

TEST(DeviceAuth, ForeignKeyRejected)
{
    DeviceAuth::DeviceKey key_a = DeviceAuth::generateDeviceKey();
    DeviceAuth::DeviceKey key_b = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key_a.isValid());
    ASSERT_TRUE(key_b.isValid());

    const QByteArray token_id = randomTokenId();
    const QByteArray nonce = DeviceAuth::generateServerNonce();
    const QByteArray sig = DeviceAuth::signChallenge(key_a.private_key, token_id, nonce);

    EXPECT_FALSE(DeviceAuth::verifyChallenge(key_b.public_key, token_id, nonce, sig));
}

TEST(DeviceAuth, NoncesDiffer)
{
    QByteArray a = DeviceAuth::generateServerNonce();
    QByteArray b = DeviceAuth::generateServerNonce();
    EXPECT_EQ(a.size(), DeviceAuth::kServerNonceSize);
    EXPECT_EQ(b.size(), DeviceAuth::kServerNonceSize);
    EXPECT_NE(a, b);
}

TEST(DeviceAuth, RejectsBadInputSizes)
{
    DeviceAuth::DeviceKey key = DeviceAuth::generateDeviceKey();
    ASSERT_TRUE(key.isValid());

    const QByteArray token_id = randomTokenId();
    const QByteArray nonce = DeviceAuth::generateServerNonce();
    const QByteArray sig = DeviceAuth::signChallenge(key.private_key, token_id, nonce);

    // Wrong token_id size.
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key,
        QByteArray(DeviceAuth::kTokenIdSize - 1, 0), nonce, sig));
    // Wrong nonce size.
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key, token_id,
        QByteArray(DeviceAuth::kServerNonceSize + 1, 0), sig));
    // Wrong signature size.
    EXPECT_FALSE(DeviceAuth::verifyChallenge(key.public_key, token_id, nonce,
        QByteArray(DeviceAuth::kSignatureSize - 1, 0)));
    // Wrong public key size.
    EXPECT_FALSE(DeviceAuth::verifyChallenge(QByteArray(DeviceAuth::kPublicKeySize - 1, 0),
        token_id, nonce, sig));

    // signChallenge: wrong input sizes -> empty signature.
    EXPECT_TRUE(DeviceAuth::signChallenge(key.private_key,
        QByteArray(1, 0), nonce).isEmpty());
    EXPECT_TRUE(DeviceAuth::signChallenge(key.private_key, token_id,
        QByteArray(1, 0)).isEmpty());
}
