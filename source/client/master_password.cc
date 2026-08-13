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
std::optional<QByteArray> decryptField(DataCryptor& cryptor, const QByteArray& blob)
{
    if (blob.isEmpty())
        return QByteArray();
    return cryptor.decrypt(blob);
}

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> encryptField(DataCryptor& cryptor, const QByteArray& plain)
{
    if (plain.isEmpty())
        return QByteArray();
    return cryptor.encrypt(plain);
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

    QList<HostConfig> hosts = db.allHosts();
    QList<RouterConfig> routers = db.routerList();

    // The configs hold the fields still encrypted with the current key, so they are decrypted to
    // plaintext before the key is switched. Rewriting the stored blobs as-is would leave them
    // readable only with the old key. Groups are not here: a group holds a name and a comment, and
    // neither is encrypted.
    struct HostFields { QByteArray address, username, password; };
    struct RouterFields { QByteArray address, username, password, device_token; };

    QList<HostFields> host_fields;
    QList<RouterFields> router_fields;

    for (const HostConfig& host : std::as_const(hosts))
    {
        std::optional<QByteArray> address = decryptField(cryptor, host.encryptedAddress());
        std::optional<QByteArray> username = decryptField(cryptor, host.encryptedUsername());
        std::optional<QByteArray> password = decryptField(cryptor, host.encryptedPassword());

        if (!address || !username || !password)
        {
            LOG(ERROR) << "Unable to decrypt host:" << host.id();
            return false;
        }

        host_fields.append({ *address, *username, *password });
    }

    for (const RouterConfig& router : std::as_const(routers))
    {
        std::optional<QByteArray> address = decryptField(cryptor, router.encryptedAddress());
        std::optional<QByteArray> username = decryptField(cryptor, router.encryptedUsername());
        std::optional<QByteArray> password = decryptField(cryptor, router.encryptedPassword());
        std::optional<QByteArray> device_token =
            decryptField(cryptor, router.encryptedDeviceToken());

        if (!address || !username || !password || !device_token)
        {
            LOG(ERROR) << "Unable to decrypt router:" << router.routerId();
            return false;
        }

        router_fields.append({ *address, *username, *password, *device_token });
    }

    // Switch the cryptor to the new key so the fields below are re-encrypted with it.
    cryptor.setKey(new_key);

    QList<HostConfig> new_hosts;
    new_hosts.reserve(hosts.size());

    // A field that fails to encrypt would otherwise be stored empty, and the plaintext it held is
    // gone with the old key. Nothing has been written yet, so the old key goes back and the change
    // is refused.
    for (int i = 0; i < hosts.size(); ++i)
    {
        HostConfig host = hosts[i];
        const HostFields& fields = host_fields[i];

        std::optional<QByteArray> address = encryptField(cryptor, fields.address);
        std::optional<QByteArray> username = encryptField(cryptor, fields.username);
        std::optional<QByteArray> password = encryptField(cryptor, fields.password);

        if (!address || !username || !password)
        {
            LOG(ERROR) << "Unable to re-encrypt host:" << host.id();
            cryptor.setKey(old_key);
            return false;
        }

        host.setEncryptedAddress(*address);
        host.setEncryptedUsername(*username);
        host.setEncryptedPassword(*password);

        new_hosts.append(host);
    }

    QList<RouterConfig> new_routers;
    new_routers.reserve(routers.size());

    for (int i = 0; i < routers.size(); ++i)
    {
        RouterConfig router = routers[i];
        const RouterFields& fields = router_fields[i];

        std::optional<QByteArray> address = encryptField(cryptor, fields.address);
        std::optional<QByteArray> username = encryptField(cryptor, fields.username);
        std::optional<QByteArray> password = encryptField(cryptor, fields.password);
        std::optional<QByteArray> device_token = encryptField(cryptor, fields.device_token);

        if (!address || !username || !password || !device_token)
        {
            LOG(ERROR) << "Unable to re-encrypt router:" << router.routerId();
            cryptor.setKey(old_key);
            return false;
        }

        router.setEncryptedAddress(*address);
        router.setEncryptedUsername(*username);
        router.setEncryptedPassword(*password);
        router.setEncryptedDeviceToken(*device_token);

        new_routers.append(router);
    }

    // Persisting all records together with the new verifier is atomic, so a failure cannot leave the
    // address book with some records under the old key and others under the new one.
    if (!db.reencryptAll(new_hosts, new_routers, new_salt, new_verifier, kCurrentVersion))
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
