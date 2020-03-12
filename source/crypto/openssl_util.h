//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CRYPTO__OPENSSL_UTIL_H
#define CRYPTO__OPENSSL_UTIL_H

#include "base/memory/byte_array.h"

#include <memory>

struct bignum_ctx;
struct bignum_st;
struct evp_cipher_ctx_st;
struct evp_pkey_ctx_st;
struct evp_pkey_st;

namespace crypto {

struct BIGNUM_CTX_Deleter
{
    void operator()(bignum_ctx* bignum_ctx);
};

struct BIGNUM_Deleter
{
    void operator()(bignum_st* bignum);
};

struct EVP_CIPHER_CTX_Deleter
{
    void operator()(evp_cipher_ctx_st* ctx);
};

struct EVP_PKEY_CTX_Deleter
{
    void operator()(evp_pkey_ctx_st* ctx);
};

struct EVP_PKEY_Deleter
{
    void operator()(evp_pkey_st* pkey);
};

using BIGNUM_CTX_ptr = std::unique_ptr<bignum_ctx, BIGNUM_CTX_Deleter>;
using BIGNUM_ptr = std::unique_ptr<bignum_st, BIGNUM_Deleter>;
using EVP_CIPHER_CTX_ptr = std::unique_ptr<evp_cipher_ctx_st, EVP_CIPHER_CTX_Deleter>;
using EVP_PKEY_CTX_ptr = std::unique_ptr<evp_pkey_ctx_st, EVP_PKEY_CTX_Deleter>;
using EVP_PKEY_ptr = std::unique_ptr<evp_pkey_st, EVP_PKEY_Deleter>;

enum class CipherType
{
    AES256_GCM,
    CHACHA20_POLY1305
};

enum class CipherMode
{
    ENCRYPT,
    DECRYPT
};

EVP_CIPHER_CTX_ptr createCipher(
    CipherType type, CipherMode mode, const base::ByteArray& key, int iv_size);

} // namespace crypto

#endif // CRYPTO__OPENSSL_UTIL_H
