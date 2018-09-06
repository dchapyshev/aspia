//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "crypto/encryptor.h"

#include "base/message_serialization.h"
#include "crypto/secure_memory.h"
#include "protocol/key_exchange.pb.h"

#define SODIUM_STATIC
#include <sodium.h>

namespace aspia {

Encryptor::Encryptor(const std::string& key,
                     const std::string& encrypt_iv,
                     const std::string& decrypt_iv)
    : key_(key),
      encrypt_iv_(encrypt_iv),
      decrypt_iv_(decrypt_iv)
{
    // Nothing
}

Encryptor::~Encryptor()
{
    secureMemZero(&key_);
    secureMemZero(&encrypt_iv_);
    secureMemZero(&decrypt_iv_);
}

// static
Encryptor* Encryptor::create(const std::string& key,
                             const std::string& encrypt_iv,
                             const std::string& decrypt_iv)
{
    if (key.size() != crypto_kx_SESSIONKEYBYTES)
        return nullptr;

    if (encrypt_iv.size() != crypto_secretbox_NONCEBYTES)
        return nullptr;

    if (decrypt_iv.size() != crypto_secretbox_NONCEBYTES)
        return nullptr;

    return new Encryptor(key, encrypt_iv, decrypt_iv);
}

int Encryptor::encryptedDataSize(int source_data_size)
{
    return source_data_size + crypto_secretbox_MACBYTES;
}

bool Encryptor::encrypt(const char* source, int source_size, char* target)
{
    Q_ASSERT(encrypt_iv_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!key_.empty());

    sodium_increment(reinterpret_cast<uint8_t*>(encrypt_iv_.data()),
                     crypto_secretbox_NONCEBYTES);

    // Encrypt message.
    if (crypto_secretbox_easy(reinterpret_cast<uint8_t*>(target),
                              reinterpret_cast<const uint8_t*>(source),
                              source_size,
                              reinterpret_cast<const uint8_t*>(encrypt_iv_.data()),
                              reinterpret_cast<const uint8_t*>(key_.data())) != 0)
    {
        qWarning("crypto_secretbox_easy failed");
        return false;
    }

    return true;
}

int Encryptor::decryptedDataSize(int source_data_size)
{
    return source_data_size - crypto_secretbox_MACBYTES;
}

bool Encryptor::decrypt(const char* source, int source_size, char* target)
{
    Q_ASSERT(decrypt_iv_.size() == crypto_secretbox_NONCEBYTES);
    Q_ASSERT(!key_.empty());

    sodium_increment(reinterpret_cast<uint8_t*>(decrypt_iv_.data()), crypto_secretbox_NONCEBYTES);

    // Decrypt message.
    if (crypto_secretbox_open_easy(reinterpret_cast<uint8_t*>(target),
                                   reinterpret_cast<const uint8_t*>(source),
                                   source_size,
                                   reinterpret_cast<const uint8_t*>(decrypt_iv_.data()),
                                   reinterpret_cast<const uint8_t*>(key_.data())) != 0)
    {
        qWarning("crypto_secretbox_open_easy failed");
        return false;
    }

    return true;
}

} // namespace aspia
