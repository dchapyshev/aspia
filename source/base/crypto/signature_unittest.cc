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

#include "base/crypto/signature.h"

#include "base/crypto/secure_byte_array.h"

#include <gtest/gtest.h>

namespace {

// Test vectors 1 and 2 from RFC 8032, section 7.1.
const char kPrivateKey1[] = "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
const char kPublicKey1[] = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
const char kMessage1[] = "";
const char kSignature1[] =
    "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b"
    "46bd25bf5f0595bbe24655141438e7a100b";

const char kPrivateKey2[] = "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb";
const char kPublicKey2[] = "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c";
const char kMessage2[] = "72";
const char kSignature2[] =
    "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11"
    "d8c387b2eaeb4302aeeb00d291612bb0c00";

//--------------------------------------------------------------------------------------------------
SecureByteArray privateKey(const char* hex)
{
    return SecureByteArray(QByteArray::fromHex(hex));
}

} // namespace

TEST(Signature, PublicKey)
{
    EXPECT_EQ(Signature::publicKey(privateKey(kPrivateKey1)), QByteArray::fromHex(kPublicKey1));
    EXPECT_EQ(Signature::publicKey(privateKey(kPrivateKey2)), QByteArray::fromHex(kPublicKey2));

    EXPECT_TRUE(Signature::publicKey(SecureByteArray()).isEmpty());
    EXPECT_TRUE(Signature::publicKey(privateKey("9d61b19deffd5a60")).isEmpty());
}

TEST(Signature, Rfc8032Vector1)
{
    QByteArray message = QByteArray::fromHex(kMessage1);
    QByteArray signature = QByteArray::fromHex(kSignature1);

    EXPECT_EQ(Signature::create(privateKey(kPrivateKey1), message), signature);
    EXPECT_TRUE(Signature::verify(QByteArray::fromHex(kPublicKey1), message, signature));
}

TEST(Signature, Rfc8032Vector2)
{
    QByteArray message = QByteArray::fromHex(kMessage2);
    QByteArray signature = QByteArray::fromHex(kSignature2);

    EXPECT_EQ(Signature::create(privateKey(kPrivateKey2), message), signature);
    EXPECT_TRUE(Signature::verify(QByteArray::fromHex(kPublicKey2), message, signature));
}

TEST(Signature, ModifiedData)
{
    QByteArray public_key = QByteArray::fromHex(kPublicKey1);
    QByteArray data("{ \"format\": 1 }");

    QByteArray signature = Signature::create(privateKey(kPrivateKey1), data);
    ASSERT_TRUE(Signature::verify(public_key, data, signature));

    data[3] = data[3] + 1;
    EXPECT_FALSE(Signature::verify(public_key, data, signature));

    EXPECT_FALSE(Signature::verify(public_key, QByteArray(), signature));
}

TEST(Signature, ModifiedSignature)
{
    QByteArray public_key = QByteArray::fromHex(kPublicKey1);
    QByteArray data("{ \"format\": 1 }");

    QByteArray signature = Signature::create(privateKey(kPrivateKey1), data);
    ASSERT_TRUE(Signature::verify(public_key, data, signature));

    signature[0] = signature[0] + 1;
    EXPECT_FALSE(Signature::verify(public_key, data, signature));

    signature[0] = signature[0] - 1;
    signature[63] = signature[63] + 1;
    EXPECT_FALSE(Signature::verify(public_key, data, signature));
}

TEST(Signature, AnotherKey)
{
    QByteArray data("{ \"format\": 1 }");
    QByteArray signature = Signature::create(privateKey(kPrivateKey1), data);

    EXPECT_FALSE(Signature::verify(QByteArray::fromHex(kPublicKey2), data, signature));
}

TEST(Signature, InvalidSizes)
{
    QByteArray public_key = QByteArray::fromHex(kPublicKey1);
    QByteArray data("{ \"format\": 1 }");
    QByteArray signature = Signature::create(privateKey(kPrivateKey1), data);

    EXPECT_TRUE(Signature::create(SecureByteArray(), data).isEmpty());
    EXPECT_TRUE(Signature::create(privateKey("9d61b19deffd5a60"), data).isEmpty());

    EXPECT_FALSE(Signature::verify(QByteArray(), data, signature));
    EXPECT_FALSE(Signature::verify(public_key.left(31), data, signature));
    EXPECT_FALSE(Signature::verify(QByteArray(public_key + "0"), data, signature));

    EXPECT_FALSE(Signature::verify(public_key, data, QByteArray()));
    EXPECT_FALSE(Signature::verify(public_key, data, signature.left(63)));
    EXPECT_FALSE(Signature::verify(public_key, data, QByteArray(signature + "0")));
}
