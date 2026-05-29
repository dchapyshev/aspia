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
bool changeKeyAndReencrypt(const SecureByteArray& new_key, const QByteArray& new_salt, const QByteArray& new_verifier)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();
    SecureByteArray old_key = cryptor.key();

    // Read decrypted data with the current key.
    QList<HostConfig> hosts = db.allHosts();
    QList<GroupConfig> groups = db.allGroups();
    QList<RouterConfig> routers = db.routerList();

    // Switch the cryptor to the new key so writes are encrypted with it.
    cryptor.setKey(new_key);

    for (HostConfig host : std::as_const(hosts))
    {
        if (!db.modifyHost(host))
        {
            LOG(ERROR) << "Unable to re-encrypt host:" << host.id();
            cryptor.setKey(old_key);
            return false;
        }
    }

    for (const GroupConfig& group : std::as_const(groups))
    {
        if (!db.modifyGroup(group))
        {
            LOG(ERROR) << "Unable to re-encrypt group:" << group.id();
            cryptor.setKey(old_key);
            return false;
        }
    }

    for (const RouterConfig& router : std::as_const(routers))
    {
        if (!db.modifyRouter(router))
        {
            LOG(ERROR) << "Unable to re-encrypt router:" << router.routerId();
            cryptor.setKey(old_key);
            return false;
        }
    }

    if (!db.setMasterPassword(new_salt, new_verifier, kCurrentVersion))
    {
        cryptor.setKey(old_key);
        return false;
    }

    return true;
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
bool MasterPassword::unlock(const SecureString& password)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QByteArray salt = db.masterPasswordSalt();
    QByteArray verifier = db.masterPasswordVerifier();

    if (salt.isEmpty() || verifier.isEmpty())
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

    SecureByteArray key = deriveKey(password, salt);
    if (!checkVerifier(key, verifier))
    {
        LOG(INFO) << "Invalid master password";
        return false;
    }

    DataCryptor::instance().setKey(key);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::setNew(const SecureString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return false;
    }

    if (isSet())
    {
        LOG(ERROR) << "Master password is already set";
        return false;
    }

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return false;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}

//--------------------------------------------------------------------------------------------------
// static
bool MasterPassword::change(const SecureString& current_password, const SecureString& new_password)
{
    if (new_password.isEmpty())
    {
        LOG(ERROR) << "Empty password";
        return false;
    }

    if (!unlock(current_password))
        return false;

    QByteArray salt = Random::byteArray(kSaltSize);
    CHECK(!salt.isEmpty());

    SecureByteArray new_key = deriveKey(new_password, salt);
    std::optional<QByteArray> verifier = makeVerifier(new_key);
    if (!verifier.has_value())
        return false;

    return changeKeyAndReencrypt(new_key, salt, *verifier);
}
