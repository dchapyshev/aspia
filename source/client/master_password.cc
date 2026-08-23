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
bool changeKeyAndReencrypt(const SecureByteArray& new_key, const QByteArray& new_salt,
                           const QByteArray& new_verifier)
{
    Database& db = Database::instance();
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    DataCryptor& cryptor = DataCryptor::instance();
    const SecureByteArray old_key = cryptor.key();

    // Reading opens the sealed column of every record with the current key, so from here on the
    // records carry their credentials in the clear and the key can be changed under them. Groups are
    // not here: a group holds a name and a comment, and neither is encrypted.
    // A truncated list would leave the records it misses sealed with the old key forever, so the
    // key is not changed unless every record was read.
    QList<LocalHostConfig> local_hosts;
    QList<RouterConfig> routers;
    QList<RouterHostConfig> router_hosts;
    if (!db.allLocalHosts(&local_hosts) || !db.routerList(&routers) || !db.allRouterHosts(&router_hosts))
    {
        LOG(ERROR) << "Unable to read the address book completely";
        return false;
    }

    // A record whose column refused to open comes back with its credentials empty, and writing it
    // out again would make that emptiness permanent. A stored record always has these fields,
    // because addLocalHost(), addRouter() and addRouterHost() refuse a record without them, so
    // empty here can only mean the column did not open.
    for (const LocalHostConfig& local_host : std::as_const(local_hosts))
    {
        if (local_host.address().isEmpty())
        {
            LOG(ERROR) << "Unable to read credentials of host:" << local_host.id();
            return false;
        }
    }

    for (const RouterConfig& router : std::as_const(routers))
    {
        if (router.address().isEmpty() || router.username().isEmpty())
        {
            LOG(ERROR) << "Unable to read credentials of router:" << router.routerId();
            return false;
        }
    }

    for (const RouterHostConfig& router_host : std::as_const(router_hosts))
    {
        if (router_host.username().isEmpty())
        {
            LOG(ERROR) << "Unable to read credentials of router host:" << router_host.hostId();
            return false;
        }
    }

    // Everything below is sealed with the new key: the records seal themselves on the way into the
    // database, and they take the key from the cryptor at that moment.
    cryptor.setKey(new_key);

    // Persisting all records together with the new verifier is atomic, so a failure cannot leave the
    // address book with some records under the old key and others under the new one.
    if (!db.reencryptAll(local_hosts, routers, router_hosts, new_salt, new_verifier,
                         kCurrentVersion))
    {
        // Nothing was written, so restore the in-memory key to keep it consistent with the database.
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
