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

#include "client/sync/sync_crypto.h"

#include <gtest/gtest.h>

#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/secure_string.h"
#include "proto/router_constants.h"

namespace {

const proto::router::SyncDataType kType = proto::router::SYNC_DATA_TYPE_CREDENTIALS;
const proto::router::SyncDataType kOtherType = static_cast<proto::router::SyncDataType>(kType + 1);
const char kKeyAad[] = "key";

//--------------------------------------------------------------------------------------------------
SecureString password()
{
    return SecureString(QString("Password123"));
}

//--------------------------------------------------------------------------------------------------
SecureString otherPassword()
{
    return SecureString(QString("Password124"));
}

//--------------------------------------------------------------------------------------------------
// Seals |payload| the way |wrapped| was sealed, so that a test can hand over what a wrap would
// never make.
std::string sealLike(const proto::router::SyncStorageKey& wrapped, const QByteArray& payload)
{
    DataCryptor cryptor(SecureByteArray(
        PasswordHash::hash(PasswordHash::ARGON2ID, password(), QByteArray::fromStdString(wrapped.salt()))));
    return cryptor.encrypt(payload, kKeyAad).value_or(QByteArray()).toStdString();
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, GeneratedKeysDiffer)
{
    const SecureByteArray first = SyncCrypto::generateStorageKey();
    const SecureByteArray second = SyncCrypto::generateStorageKey();

    EXPECT_EQ(first.size(), 32);
    EXPECT_EQ(second.size(), 32);
    EXPECT_NE(first, second);
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, WrappedKeyOpensWithItsPassword)
{
    const SecureByteArray key = SyncCrypto::generateStorageKey();

    proto::router::SyncStorageKey wrapped;
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key, password(), &wrapped));

    EXPECT_EQ(wrapped.salt().size(), 32u);

    for (const std::string& field : { wrapped.salt(), wrapped.verifier(), wrapped.data() })
    {
        EXPECT_FALSE(field.empty());
        EXPECT_LE(field.size(), static_cast<size_t>(proto::router::kMaxSyncStorageKeyFieldSize));
    }

    SecureByteArray unwrapped;
    ASSERT_EQ(SyncCrypto::unwrapStorageKey(wrapped, password(), &unwrapped), SyncCrypto::Result::SUCCESS);
    EXPECT_EQ(unwrapped, key);
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, WrongPasswordIsToldApartFromDamage)
{
    const SecureByteArray key = SyncCrypto::generateStorageKey();

    proto::router::SyncStorageKey wrapped;
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key, password(), &wrapped));

    SecureByteArray unwrapped;
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(wrapped, otherPassword(), &unwrapped), SyncCrypto::Result::WRONG_PASSWORD);
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(wrapped, SecureString(), &unwrapped), SyncCrypto::Result::WRONG_PASSWORD);

    proto::router::SyncStorageKey damaged = wrapped;
    damaged.mutable_data()->back() ^= 1;
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(damaged, password(), &unwrapped), SyncCrypto::Result::INVALID_FORMAT);

    damaged = wrapped;
    damaged.set_data(sealLike(wrapped, QByteArray(16, 'k')));
    ASSERT_FALSE(damaged.data().empty());
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(damaged, password(), &unwrapped), SyncCrypto::Result::INVALID_FORMAT);

    EXPECT_TRUE(unwrapped.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, SwappedVerifierAndDataDoNotOpen)
{
    proto::router::SyncStorageKey wrapped;
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(SyncCrypto::generateStorageKey(), password(), &wrapped));

    proto::router::SyncStorageKey swapped = wrapped;
    swapped.set_verifier(wrapped.data());
    swapped.set_data(wrapped.verifier());

    SecureByteArray unwrapped;
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(swapped, password(), &unwrapped), SyncCrypto::Result::WRONG_PASSWORD);
    EXPECT_TRUE(unwrapped.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, IncompleteWrappedKeyIsRefused)
{
    const SecureByteArray key = SyncCrypto::generateStorageKey();

    proto::router::SyncStorageKey wrapped;
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key, password(), &wrapped));

    SecureByteArray unwrapped;

    proto::router::SyncStorageKey incomplete = wrapped;
    incomplete.mutable_salt()->pop_back();
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(incomplete, password(), &unwrapped), SyncCrypto::Result::INVALID_FORMAT);

    incomplete = wrapped;
    incomplete.clear_verifier();
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(incomplete, password(), &unwrapped), SyncCrypto::Result::INVALID_FORMAT);

    incomplete = wrapped;
    incomplete.clear_data();
    EXPECT_EQ(SyncCrypto::unwrapStorageKey(incomplete, password(), &unwrapped), SyncCrypto::Result::INVALID_FORMAT);

    EXPECT_EQ(SyncCrypto::unwrapStorageKey(proto::router::SyncStorageKey(), password(), &unwrapped),
              SyncCrypto::Result::INVALID_FORMAT);
    EXPECT_TRUE(unwrapped.isEmpty());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, SameKeyIsWrappedDifferentlyEachTime)
{
    const SecureByteArray key = SyncCrypto::generateStorageKey();

    proto::router::SyncStorageKey first;
    proto::router::SyncStorageKey second;
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key, password(), &first));
    ASSERT_TRUE(SyncCrypto::wrapStorageKey(key, password(), &second));

    EXPECT_NE(first.salt(), second.salt());
    EXPECT_NE(first.verifier(), second.verifier());
    EXPECT_NE(first.data(), second.data());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, InvalidKeyOrPasswordIsNotWrapped)
{
    proto::router::SyncStorageKey wrapped;

    EXPECT_FALSE(SyncCrypto::wrapStorageKey(SyncCrypto::generateStorageKey(), SecureString(), &wrapped));
    EXPECT_FALSE(SyncCrypto::wrapStorageKey(SecureByteArray(), password(), &wrapped));
    EXPECT_FALSE(SyncCrypto::wrapStorageKey(SecureByteArray(QByteArray(16, 'k')), password(), &wrapped));

    EXPECT_TRUE(wrapped.salt().empty());
    EXPECT_TRUE(wrapped.verifier().empty());
    EXPECT_TRUE(wrapped.data().empty());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, RecordOpensOnlyInItsPlace)
{
    const SecureByteArray key = SyncCrypto::generateStorageKey();
    const QByteArray data("record data");

    const std::optional<QByteArray> blob = SyncCrypto::encryptRecord(key, kType, "a", data);
    ASSERT_TRUE(blob.has_value());
    EXPECT_NE(*blob, data);

    const std::optional<QByteArray> opened = SyncCrypto::decryptRecord(key, kType, "a", *blob);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, data);

    EXPECT_FALSE(SyncCrypto::decryptRecord(key, kType, "b", *blob).has_value());
    EXPECT_FALSE(SyncCrypto::decryptRecord(key, kOtherType, "a", *blob).has_value());
    EXPECT_FALSE(SyncCrypto::decryptRecord(SyncCrypto::generateStorageKey(), kType, "a", *blob).has_value());
}

//--------------------------------------------------------------------------------------------------
TEST(SyncCryptoTest, RecordIsNotSealedWithoutAKey)
{
    EXPECT_FALSE(SyncCrypto::encryptRecord(SecureByteArray(), kType, "a", QByteArray("data")).has_value());
    EXPECT_FALSE(SyncCrypto::decryptRecord(SecureByteArray(), kType, "a", QByteArray("data")).has_value());
}
