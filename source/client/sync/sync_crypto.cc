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

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"

namespace {

const int kKeySize = 32;
const int kSaltSize = 32;
const int kVerifierPayloadSize = 32;
const char kVerifierAad[] = "verifier";
const char kKeyAad[] = "key";

//--------------------------------------------------------------------------------------------------
SecureByteArray passwordKey(const SecureString& password, const QByteArray& salt)
{
    return SecureByteArray(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
}

//--------------------------------------------------------------------------------------------------
QByteArray recordAad(proto::router::SyncDataType type, std::string_view id)
{
    return QByteArray::number(static_cast<int>(type)) + '/' +
        QByteArray(id.data(), static_cast<qsizetype>(id.size()));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
SecureByteArray SyncCrypto::generateStorageKey()
{
    return SecureByteArray(Random::byteArray(kKeySize));
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncCrypto::wrapStorageKey(
    const SecureByteArray& key, const SecureString& password, proto::router::SyncStorageKey* wrapped)
{
    CHECK(wrapped);

    if (key.size() != kKeySize || password.isEmpty())
    {
        LOG(ERROR) << "Invalid key or password";
        return false;
    }

    const QByteArray salt = Random::byteArray(kSaltSize);
    if (salt.size() != kSaltSize)
    {
        LOG(ERROR) << "Unable to make a salt";
        return false;
    }

    DataCryptor cryptor(passwordKey(password, salt));

    const std::optional<QByteArray> verifier = cryptor.encrypt(Random::byteArray(kVerifierPayloadSize), kVerifierAad);
    const std::optional<QByteArray> data = cryptor.encrypt(key.toByteArray(), kKeyAad);

    if (!verifier.has_value() || !data.has_value())
    {
        LOG(ERROR) << "Unable to wrap the key";
        return false;
    }

    wrapped->set_salt(salt.toStdString());
    wrapped->set_verifier(verifier->toStdString());
    wrapped->set_data(data->toStdString());
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
SyncCrypto::Result SyncCrypto::unwrapStorageKey(
    const proto::router::SyncStorageKey& wrapped, const SecureString& password, SecureByteArray* key)
{
    CHECK(key);

    const QByteArray salt = QByteArray::fromStdString(wrapped.salt());
    if (salt.size() != kSaltSize || wrapped.verifier().empty() || wrapped.data().empty())
        return Result::INVALID_FORMAT;

    if (password.isEmpty())
        return Result::WRONG_PASSWORD;

    DataCryptor cryptor(passwordKey(password, salt));

    if (!cryptor.decrypt(QByteArray::fromStdString(wrapped.verifier()), kVerifierAad).has_value())
        return Result::WRONG_PASSWORD;

    std::optional<QByteArray> decrypted = cryptor.decrypt(QByteArray::fromStdString(wrapped.data()), kKeyAad);
    if (!decrypted.has_value())
    {
        // The password is already proven right, so this is damage.
        LOG(ERROR) << "Unable to unwrap the key";
        return Result::INVALID_FORMAT;
    }

    SecureByteArray unwrapped(std::move(*decrypted));
    if (unwrapped.size() != kKeySize)
    {
        LOG(ERROR) << "Wrong size of the unwrapped key:" << unwrapped.size();
        return Result::INVALID_FORMAT;
    }

    *key = std::move(unwrapped);
    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncCrypto::encryptRecord(
    const SecureByteArray& key, proto::router::SyncDataType type, std::string_view id, QByteArrayView data)
{
    return DataCryptor(key).encrypt(data, recordAad(type, id));
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncCrypto::decryptRecord(
    const SecureByteArray& key, proto::router::SyncDataType type, std::string_view id, QByteArrayView blob)
{
    return DataCryptor(key).decrypt(blob, recordAad(type, id));
}
