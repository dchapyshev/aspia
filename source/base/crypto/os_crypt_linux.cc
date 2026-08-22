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

#include "base/crypto/os_crypt.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/secure_byte_array.h"

// Linux (Android included) has no keystore every device exposes to native code, so the wrap key
// is derived from the identity of the machine instead. A copy of the storage taken to another
// machine does not open; on the machine itself the identity is readable and guards nothing.

namespace {

// Salts the derivation. The bare identity cannot be the key, because the identity of a machine
// that runs a host leaves for its router as hw_id.
const char kContext[] = "base::OSCrypt::key";

// Authenticated by the cipher, so a blob sealed for another consumer of the key does not open.
const char kAad[] = "base::OSCrypt";

//--------------------------------------------------------------------------------------------------
// Derives the wrap key from the identity of the machine. Empty when there is no identity to
// derive from.
SecureByteArray wrappingKey()
{
    const QByteArray hardware_id = SysInfo::hardwareId();
    if (hardware_id.isEmpty())
    {
        LOG(ERROR) << "No hardware identity to derive the key from";
        return SecureByteArray();
    }

    return SecureByteArray(GenericHash::hash(GenericHash::SHA256, QByteArray(kContext) + hardware_id));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::encryptString(const QString& plaintext, QByteArray* ciphertext)
{
    return encryptBytes(plaintext.toUtf8(), ciphertext);
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::decryptString(const QByteArray& ciphertext, QString* plaintext)
{
    QByteArray bytes;
    if (!decryptBytes(ciphertext, &bytes))
        return false;

    *plaintext = QString::fromUtf8(bytes);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::encryptBytes(const QByteArray& plaintext, QByteArray* ciphertext)
{
    if (plaintext.isEmpty())
    {
        *ciphertext = QByteArray();
        return true;
    }

    const SecureByteArray key = wrappingKey();
    if (key.isEmpty())
        return false;

    const DataCryptor cryptor(CipherType::AES256_GCM, key);
    std::optional<QByteArray> sealed = cryptor.encrypt(plaintext, kAad);
    if (!sealed.has_value())
    {
        LOG(ERROR) << "Failed to encrypt";
        return false;
    }

    *ciphertext = std::move(*sealed);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool OSCrypt::decryptBytes(const QByteArray& ciphertext, QByteArray* plaintext)
{
    if (ciphertext.isEmpty())
    {
        *plaintext = QByteArray();
        return true;
    }

    const SecureByteArray key = wrappingKey();
    if (key.isEmpty())
        return false;

    const DataCryptor cryptor(CipherType::AES256_GCM, key);
    std::optional<QByteArray> opened = cryptor.decrypt(ciphertext, kAad);
    if (!opened.has_value())
    {
        LOG(ERROR) << "Failed to decrypt";
        return false;
    }

    *plaintext = std::move(*opened);
    return true;
}
