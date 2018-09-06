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

#include "crypto/generic_hash.h"

#include <openssl/evp.h>

namespace aspia {

GenericHash::GenericHash(Type type)
{
    ctxt_ = EVP_MD_CTX_new();
    if (!ctxt_)
    {
        qFatal("EVP_MD_CTX_new failed");
        return;
    }

    switch (type)
    {
        case SHA1:
            md_ = EVP_sha1();
            break;

        case BLAKE2b512:
            md_ = EVP_blake2b512();
            break;

        case BLAKE2s256:
            md_ = EVP_blake2s256();
            break;

        case SHA224:
            md_ = EVP_sha224();
            break;

        case SHA256:
            md_ = EVP_sha256();
            break;

        case SHA384:
            md_ = EVP_sha384();
            break;

        case SHA512:
            md_ = EVP_sha512();
            break;

        default:
            qFatal("Unknown hashing algorithm");
            return;
    }

    reset();
}

GenericHash::~GenericHash()
{
    Q_ASSERT(ctxt_);
    EVP_MD_CTX_free(ctxt_);
}

// static
std::string GenericHash::hash(Type type, const void* data, size_t size)
{
    GenericHash generic_hash(type);
    generic_hash.addData(data, size);
    return generic_hash.result();
}

// static
std::string GenericHash::hash(Type type, const std::string& data)
{
    return hash(type, data.c_str(), data.size());
}

void GenericHash::addData(const void* data, size_t size)
{
    Q_ASSERT(ctxt_);

    if (!EVP_DigestUpdate(ctxt_, data, size))
    {
        qFatal("EVP_DigestUpdate failed");
        return;
    }
}

void GenericHash::addData(const std::string& data)
{
    addData(data.c_str(), data.size());
}

std::string GenericHash::result() const
{
    Q_ASSERT(ctxt_);
    Q_ASSERT(md_);

    int len = EVP_MD_size(md_);
    if (len <= 0)
        return std::string();

    std::string ret;
    ret.resize(len);

    if (!EVP_DigestFinal(ctxt_, reinterpret_cast<uint8_t*>(ret.data()), nullptr))
    {
        qFatal("EVP_DigestFinal failed");
        return std::string();
    }

    return ret;
}

void GenericHash::reset()
{
    if (!EVP_DigestInit(ctxt_, md_))
    {
        qFatal("EVP_DigestInit failed");
        return;
    }
}

} // namespace aspia
