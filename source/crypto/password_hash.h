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

#ifndef CRYPTO__PASSWORD_HASH_H
#define CRYPTO__PASSWORD_HASH_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"

namespace crypto {

class PasswordHash
{
public:
    enum Type { SCRYPT };

    static const size_t kBitsPerByte = 8;
    static const size_t kBitsSize = 256;
    static const size_t kBytesSize = kBitsSize / kBitsPerByte;

    static base::ByteArray hash(
        Type type, std::string_view password, const base::ByteArray& salt);

    static std::string hash(Type type, std::string_view password, std::string_view salt);

private:
    DISALLOW_COPY_AND_ASSIGN(PasswordHash);
};

} // namespace crypto

#endif // CRYPTO__PASSWORD_HASH_H
