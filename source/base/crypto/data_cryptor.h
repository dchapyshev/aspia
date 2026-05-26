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

#ifndef BASE_CRYPTO_DATA_CRYPTOR_H
#define BASE_CRYPTO_DATA_CRYPTOR_H

#include "base/crypto/openssl_util.h"
#include "base/crypto/secure_byte_array.h"

#include <QByteArray>
#include <QByteArrayView>

#include <mutex>
#include <optional>

// Thread-safe AEAD wrapper around ChaCha20-Poly1305. The singleton accessed via instance()
// holds a process-wide key shared between all threads. EVP_CIPHER_CTX is not thread-safe, so
// the cipher state is guarded by a mutex - encrypt() and decrypt() are serialized across
// threads. For our workload (single-digit calls per second) this is negligible. Per-instance
// (non-singleton) cryptors are also serialized for consistency.
class DataCryptor
{
public:
    DataCryptor();
    explicit DataCryptor(const SecureByteArray& key);
    DataCryptor(DataCryptor&& other) noexcept;
    DataCryptor& operator=(DataCryptor&& other) noexcept;
    ~DataCryptor();

    void setKey(const SecureByteArray& key);
    SecureByteArray key() const;

    bool isValid() const;

    std::optional<QByteArray> encrypt(QByteArrayView in) const;
    std::optional<QByteArray> decrypt(QByteArrayView in) const;

    static DataCryptor& instance();

private:
    mutable std::mutex mutex_;
    SecureByteArray key_;
    EVP_CIPHER_CTX_ptr encrypt_ctx_;
    EVP_CIPHER_CTX_ptr decrypt_ctx_;

    Q_DISABLE_COPY(DataCryptor)
};

#endif // BASE_CRYPTO_DATA_CRYPTOR_H
