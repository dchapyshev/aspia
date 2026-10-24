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

#include "host/host_identity.h"

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "host/database.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <dpapi.h>
#endif

namespace {

#if defined(Q_OS_WINDOWS)
const char kProtectedPrefix[] = "DPAPI1";
#endif

//--------------------------------------------------------------------------------------------------
// Turns the private key into what is stored in the database.
QByteArray protectKey(const SecureByteArray& private_key)
{
#if defined(Q_OS_WINDOWS)
    DATA_BLOB in;
    in.cbData = static_cast<DWORD>(private_key.size());
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(private_key.constData()));

    DATA_BLOB out = { 0, nullptr };
    if (!CryptProtectData(&in, L"Aspia host identity key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        PLOG(ERROR) << "CryptProtectData failed";
        return QByteArray();
    }

    QByteArray result(kProtectedPrefix);
    result.append(reinterpret_cast<const char*>(out.pbData), static_cast<qsizetype>(out.cbData));
    LocalFree(out.pbData);
    return result;
#else
    return private_key.toByteArray();
#endif
}

//--------------------------------------------------------------------------------------------------
// Reverses protectKey(). |legacy| is set for a key stored without protection by an older version.
SecureByteArray unprotectKey(const QByteArray& stored, bool* legacy)
{
    *legacy = false;

#if defined(Q_OS_WINDOWS)
    if (stored.startsWith(kProtectedPrefix))
    {
        const qsizetype prefix_size = static_cast<qsizetype>(sizeof(kProtectedPrefix) - 1);

        DATA_BLOB in;
        in.cbData = static_cast<DWORD>(stored.size() - prefix_size);
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(stored.constData() + prefix_size));

        DATA_BLOB out = { 0, nullptr };
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &out))
        {
            PLOG(ERROR) << "CryptUnprotectData failed";
            return SecureByteArray();
        }

        SecureByteArray result(reinterpret_cast<const char*>(out.pbData),
                               static_cast<qsizetype>(out.cbData));
        SecureZeroMemory(out.pbData, out.cbData);
        LocalFree(out.pbData);
        return result;
    }

    // Stored by v3.1.3-rc1 without protection; re-stored protected by the caller.
    *legacy = true;
#endif

    return SecureByteArray(stored);
}

//--------------------------------------------------------------------------------------------------
bool storeKey(Database& database, const SecureByteArray& private_key)
{
    QByteArray stored = protectKey(private_key);
    const bool ok = !stored.isEmpty() && database.setIdentityPrivateKey(stored);
    memZero(&stored);
    return ok;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
SecureByteArray HostIdentity::privateKey(Database& database)
{
    if (!database.isValid())
        return SecureByteArray();

    QByteArray stored = database.identityPrivateKey();
    if (stored.isEmpty())
    {
        LOG(INFO) << "No identity key yet";
        if (!regenerate(database))
            return SecureByteArray();

        stored = database.identityPrivateKey();
    }

    bool legacy = false;
    SecureByteArray private_key = unprotectKey(stored, &legacy);
    memZero(&stored);

    if (private_key.isEmpty() || !KeyPair::fromPrivateKey(private_key).isValid())
    {
        // Never replaced silently: a new key would make every client accept whatever key the host
        // presents next time without the administrator knowing why.
        LOG(ERROR) << "The stored identity key can not be read (damaged or protected on another "
                      "computer). Operating system / domain accounts are unavailable until the key "
                      "is regenerated in the host settings (Domain tab)";
        return SecureByteArray();
    }

    if (legacy && !storeKey(database, private_key))
        LOG(WARNING) << "Unable to store the identity key protected";

    return private_key;
}

//--------------------------------------------------------------------------------------------------
// static
bool HostIdentity::regenerate(Database& database)
{
    if (!database.isValid())
        return false;

    KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        LOG(ERROR) << "Unable to generate the identity key";
        return false;
    }

    SecureByteArray private_key = key_pair.privateKey();
    if (private_key.isEmpty() || !storeKey(database, private_key))
    {
        LOG(ERROR) << "Unable to store the identity key";
        return false;
    }

    LOG(INFO) << "New identity key generated";
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray HostIdentity::publicKey(Database& database)
{
    SecureByteArray private_key = privateKey(database);
    if (private_key.isEmpty())
        return QByteArray();

    return KeyPair::fromPrivateKey(private_key).publicKey();
}
