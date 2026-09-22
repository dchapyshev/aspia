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

#include "client/master_password.h"

#include <optional>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "client/database.h"

namespace {

const quint32 kCurrentVersion = 1;
const int kSaltSize = 32;
const int kVerifierPayloadSize = 32;

//--------------------------------------------------------------------------------------------------
SecureByteArray deriveKey(const SecureString& password, const QByteArray& salt)
{
    return SecureByteArray(PasswordHash::hash(PasswordHash::ARGON2ID, password, salt));
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> makeVerifier(const SecureByteArray& key)
{
    return DataCryptor(CipherType::AES256_GCM, key)
        .encrypt(Random::byteArray(kVerifierPayloadSize));
}

//--------------------------------------------------------------------------------------------------
bool checkVerifier(const SecureByteArray& key, const QByteArray& verifier)
{
    // AES256 GCM is an AEAD cipher: successful decryption (i.e. valid auth tag)
    // is itself the proof that the key is correct. The plaintext content does not matter.
    return DataCryptor(CipherType::AES256_GCM, key).decrypt(verifier).has_value();
}

//--------------------------------------------------------------------------------------------------
MasterPassword::Result changeKeyAndReencrypt(
    const SecureByteArray& new_key, const QByteArray& new_salt, const QByteArray& new_verifier)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return MasterPassword::Result::FAILED;
    }

    DataCryptor& cryptor = DataCryptor::instance();
    const SecureByteArray old_key = cryptor.key();

    QList<LocalHostConfig> local_hosts;
    QList<RouterConfig> routers;
    QList<RouterHostConfig> router_hosts;
    QList<CredentialConfig> credentials;

    const Database::ReadResult read_results[] =
    {
        db.allLocalHosts(&local_hosts),
        db.routerList(&routers),
        db.allRouterHosts(&router_hosts),
        db.credentialList(&credentials)
    };

    bool incomplete = false;

    for (Database::ReadResult read_result : read_results)
    {
        if (read_result == Database::ReadResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the address book";
            return MasterPassword::Result::FAILED;
        }

        if (read_result == Database::ReadResult::INCOMPLETE)
            incomplete = true;
    }

    if (incomplete)
    {
        LOG(ERROR) << "Unable to read the address book completely";
        return MasterPassword::Result::UNREADABLE_RECORD;
    }

    cryptor.setKey(new_key);

    if (!db.reencryptAll(local_hosts, routers, router_hosts, credentials, new_salt, new_verifier,
                         kCurrentVersion))
    {
        // Nothing was written, so restore the in-memory key to keep it consistent with the database.
        cryptor.setKey(old_key);
        return MasterPassword::Result::FAILED;
    }

    return MasterPassword::Result::SUCCESS;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::isSafePassword(const SecureString& password)
{
    const QString& password_str = password.toString();
    qsizetype length = password_str.length();

    if (length < kSafePasswordLength)
        return false;

    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;

    for (qsizetype i = 0; i < length; ++i)
    {
        QChar character = password_str.at(i);

        if (character.isUpper())
            has_upper = true;

        if (character.isLower())
            has_lower = true;

        if (character.isDigit())
            has_digit = true;
    }

    return has_upper && has_lower && has_digit;
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::isSet()
{
    Database& db = Database::instance();
    if (!db.isValid())
        return false;

    return db.isMasterPasswordSet();
}

//--------------------------------------------------------------------------------------------------
// static
MasterPassword::Result MasterPassword::unlock(const SecureString& password)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Result::FAILED;
    }

    QByteArray salt = db.masterPasswordSalt();
    QByteArray verifier = db.masterPasswordVerifier();

    if (salt.isEmpty() || verifier.isEmpty())
    {
        LOG(ERROR) << "Master password is not set";
        return Result::FAILED;
    }

    quint32 version = db.masterPasswordVersion();
    if (version != kCurrentVersion)
    {
        LOG(ERROR) << "Unsupported master password version:" << version;
        return Result::FAILED;
    }

    SecureByteArray key = deriveKey(password, salt);
    if (!checkVerifier(key, verifier))
    {
        LOG(INFO) << "Invalid master password";
        return Result::INVALID_PASSWORD;
    }

    DataCryptor::instance().setKey(key);
    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::unlockWithKey(const SecureByteArray& key)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QByteArray verifier = db.masterPasswordVerifier();
    if (verifier.isEmpty())
    {
        LOG(ERROR) << "Master password is not set";
        return false;
    }

    quint32 version = db.masterPasswordVersion();
    if (version != kCurrentVersion)
    {
        LOG(ERROR) << "Unsupported master password version:" << version;
        return false;
    }

    if (!checkVerifier(key, verifier))
    {
        LOG(INFO) << "Invalid key";
        return false;
    }

    DataCryptor::instance().setKey(key);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
SecureByteArray MasterPassword::currentKey()
{
    return DataCryptor::instance().key();
}

//--------------------------------------------------------------------------------------------------
// static
MasterPassword::Result MasterPassword::setNew(const SecureString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return Result::FAILED;
    }

    if (isSet())
    {
        LOG(ERROR) << "Master password is already set";
        return Result::FAILED;
    }

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return Result::FAILED;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}

//--------------------------------------------------------------------------------------------------
// static
MasterPassword::Result MasterPassword::change(
    const SecureString& current_password, const SecureString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return Result::FAILED;
    }

    const Result unlocked = unlock(current_password);
    if (unlocked != Result::SUCCESS)
        return unlocked;

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return Result::FAILED;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}
