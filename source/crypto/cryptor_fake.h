//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CRYPTO__CRYPTOR_FAKE_H
#define CRYPTO__CRYPTOR_FAKE_H

#include "base/macros_magic.h"
#include "crypto/cryptor.h"

namespace crypto {

class CryptorFake : public Cryptor
{
public:
    CryptorFake();
    ~CryptorFake();

    size_t encryptedDataSize(size_t in_size) override;
    bool encrypt(const char* in, size_t in_size, char* out) override;

    size_t decryptedDataSize(size_t in_size) override;
    bool decrypt(const char* in, size_t in_size, char* out) override;

private:
    DISALLOW_COPY_AND_ASSIGN(CryptorFake);
};

} // namespace crypto

#endif // CRYPTO__CRYPTOR_FAKE_H
