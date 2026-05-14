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

#include <optional>

class DataCryptor
{
public:
    DataCryptor();
    explicit DataCryptor(const SecureByteArray& key);
    ~DataCryptor();

    void setKey(const SecureByteArray& key);
    SecureByteArray key() const;
    bool hasKey() const;

    std::optional<QByteArray> encrypt(QByteArrayView in) const;
    std::optional<QByteArray> decrypt(QByteArrayView in) const;

    static DataCryptor& instance();

private:
    SecureByteArray key_;
    mutable EVP_CIPHER_CTX_ptr encrypt_ctx_;
    mutable EVP_CIPHER_CTX_ptr decrypt_ctx_;

    Q_DISABLE_COPY_MOVE(DataCryptor)
};

#endif // BASE_CRYPTO_DATA_CRYPTOR_H
