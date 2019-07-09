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

#include "crypto/cryptor_fake.h"

#include <cstring>

namespace crypto {

CryptorFake::CryptorFake() = default;
CryptorFake::~CryptorFake() = default;

size_t CryptorFake::encryptedDataSize(size_t in_size)
{
    return in_size;
}

bool CryptorFake::encrypt(const char* in, size_t in_size, char* out)
{
    memcpy(out, in, in_size);
    return true;
}

size_t CryptorFake::decryptedDataSize(size_t in_size)
{
    return in_size;
}

bool CryptorFake::decrypt(const char* in, size_t in_size, char* out)
{
    memcpy(out, in, in_size);
    return true;
}

} // namespace crypto
