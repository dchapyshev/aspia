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

#ifndef CRYPTO__GENERIC_HASH_H
#define CRYPTO__GENERIC_HASH_H

#include "base/byte_array.h"
#include "base/macros_magic.h"

struct evp_md_ctx_st;
struct evp_md_st;

namespace crypto {

class GenericHash
{
public:
    enum Type
    {
        SHA1       = 1,
        BLAKE2b512 = 2,
        BLAKE2s256 = 3,
        SHA224     = 4,
        SHA256     = 5,
        SHA384     = 6,
        SHA512     = 7
    };

    GenericHash(Type type);
    ~GenericHash();

    static base::ByteArray hash(Type type, const void* data, size_t size);
    static base::ByteArray hash(Type type, std::string_view data);
    static base::ByteArray hash(Type type, const base::ByteArray& data);

    void addData(const void* data, size_t size);
    void addData(std::string_view data);
    void addData(const base::ByteArray& data);

    base::ByteArray result() const;

    void reset();

private:
    evp_md_ctx_st* ctxt_ = nullptr;
    const evp_md_st* md_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(GenericHash);
};

} // namespace crypto

#endif // CRYPTO__GENERIC_HASH_H
