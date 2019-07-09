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

#ifndef CRYPTO__DATA_CRYPTOR_CHACHA20_POLY1305_H
#define CRYPTO__DATA_CRYPTOR_CHACHA20_POLY1305_H

#include "base/macros_magic.h"
#include "crypto/data_cryptor.h"

namespace crypto {

class DataCryptorChaCha20Poly1305 : public DataCryptor
{
public:
    DataCryptorChaCha20Poly1305(std::string_view key);
    ~DataCryptorChaCha20Poly1305();

    bool encrypt(std::string_view in, std::string* out) override;
    bool decrypt(std::string_view in, std::string* out) override;

private:
    std::string key_;

    DISALLOW_COPY_AND_ASSIGN(DataCryptorChaCha20Poly1305);
};

} // namespace crypto

#endif // CRYPTO__DATA_CRYPTOR_CHACHA20_POLY1305_H
