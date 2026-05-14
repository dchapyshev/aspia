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

#include "base/crypto/generic_hash.h"

#include <openssl/evp.h>

#include "base/logging.h"
#include "base/crypto/secure_byte_array.h"

//--------------------------------------------------------------------------------------------------
GenericHash::GenericHash(Type type)
    : ctxt_(EVP_MD_CTX_new())
{
    CHECK(ctxt_) << "EVP_MD_CTX_new failed";

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
            LOG(FATAL) << "Unknown hashing algorithm";
            return;
    }

    reset();
}

//--------------------------------------------------------------------------------------------------
GenericHash::~GenericHash()
{
    DCHECK(ctxt_);
    EVP_MD_CTX_free(ctxt_);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray GenericHash::hash(Type type, const void* data, size_t size)
{
    GenericHash generic_hash(type);
    generic_hash.addData(data, size);
    return generic_hash.result();
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray GenericHash::hash(Type type, std::string_view data)
{
    return hash(type, data.data(), data.size());
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray GenericHash::hash(Type type, const QByteArray& data)
{
    return hash(type, data.data(), data.size());
}

//--------------------------------------------------------------------------------------------------
void GenericHash::addData(const void* data, size_t size)
{
    DCHECK(ctxt_);
    int ret = EVP_DigestUpdate(ctxt_, data, size);
    CHECK_EQ(ret, 1);
}

//--------------------------------------------------------------------------------------------------
void GenericHash::addData(std::string_view data)
{
    addData(data.data(), data.size());
}

//--------------------------------------------------------------------------------------------------
void GenericHash::addData(const QByteArray& data)
{
    addData(data.data(), data.size());
}

//--------------------------------------------------------------------------------------------------
void GenericHash::addData(const SecureByteArray& data)
{
    addData(data.toByteArray());
}

//--------------------------------------------------------------------------------------------------
QByteArray GenericHash::result() const
{
    DCHECK(ctxt_);
    DCHECK(md_);

    int len = EVP_MD_size(md_);
    CHECK_GT(len, 0);

    // Finalize on a copy of the context so that result() is non-destructive and can be called
    // multiple times, including interleaved with further addData() calls on the original.
    EVP_MD_CTX* ctxt_copy = EVP_MD_CTX_new();
    CHECK(ctxt_copy) << "EVP_MD_CTX_new failed";

    int ret = EVP_MD_CTX_copy_ex(ctxt_copy, ctxt_);
    CHECK_EQ(ret, 1);

    QByteArray result;
    result.resize(static_cast<qsizetype>(len));

    ret = EVP_DigestFinal_ex(ctxt_copy, reinterpret_cast<quint8*>(result.data()), nullptr);
    CHECK_EQ(ret, 1);

    EVP_MD_CTX_free(ctxt_copy);
    return result;
}

//--------------------------------------------------------------------------------------------------
void GenericHash::reset()
{
    int ret = EVP_DigestInit(ctxt_, md_);
    CHECK_EQ(ret, 1);
}
