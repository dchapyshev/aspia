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

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <mutex>
#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_byte_array.h"

namespace {

// The keychain item of the current user that holds the wrapping key.
const char kKeychainService[] = "Aspia";
const char kKeychainAccount[] = "OSCrypt";

// Authenticated by the cipher, so a blob sealed for another consumer of the key does not open.
const char kAad[] = "base::OSCrypt";

constexpr size_t kKeySize = 32;

//--------------------------------------------------------------------------------------------------
// Reads the wrapping key from the keychain of the user, creating it on first use. Empty when the
// keychain refuses. A key once read is served from memory: the keychain can lock while the client
// runs (together with the screen, by a timeout), and what worked at startup must keep working
// behind the lock. A refusal is not cached, so the next call asks the keychain again.
SecureByteArray wrappingKey()
{
    static std::mutex mutex;
    static SecureByteArray cached_key;

    const std::lock_guard lock(mutex);
    if (!cached_key.isEmpty())
        return cached_key;

    NSDictionary* query = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : @(kKeychainService),
        (__bridge id)kSecAttrAccount : @(kKeychainAccount),
        (__bridge id)kSecReturnData : @YES
    };

    CFTypeRef found = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &found);
    if (status == errSecSuccess)
    {
        CFDataRef data = static_cast<CFDataRef>(found);
        SecureByteArray key(QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                                       qsizetype(CFDataGetLength(data))));
        CFRelease(found);

        if (key.size() == qsizetype(kKeySize))
        {
            cached_key = key;
            return cached_key;
        }

        LOG(ERROR) << "Keychain item has unexpected size:" << key.size();
        return SecureByteArray();
    }

    if (status != errSecItemNotFound)
    {
        LOG(ERROR) << "SecItemCopyMatching failed:" << status;
        return SecureByteArray();
    }

    SecureByteArray key(Random::byteArray(kKeySize));

    NSDictionary* add = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : @(kKeychainService),
        (__bridge id)kSecAttrAccount : @(kKeychainAccount),
        (__bridge id)kSecAttrAccessible : (__bridge id)kSecAttrAccessibleAfterFirstUnlock,
        (__bridge id)kSecValueData : [NSData dataWithBytes:key.constData() length:kKeySize]
    };

    status = SecItemAdd((__bridge CFDictionaryRef)add, nullptr);
    if (status != errSecSuccess)
    {
        LOG(ERROR) << "SecItemAdd failed:" << status;
        return SecureByteArray();
    }

    cached_key = key;
    return cached_key;
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
