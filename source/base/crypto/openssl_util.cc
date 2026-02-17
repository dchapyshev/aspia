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

#include "base/crypto/openssl_util.h"

#include "base/logging.h"

#include <openssl/bn.h>
#include <openssl/evp.h>

namespace base {

//--------------------------------------------------------------------------------------------------
void BIGNUM_CTX_Deleter::operator()(bignum_ctx* bignum_ctx)
{
    BN_CTX_free(bignum_ctx);
}

//--------------------------------------------------------------------------------------------------
void BIGNUM_Deleter::operator()(bignum_st* bignum)
{
    BN_clear_free(bignum);
}

//--------------------------------------------------------------------------------------------------
void EVP_CIPHER_CTX_Deleter::operator()(evp_cipher_ctx_st* ctx)
{
    EVP_CIPHER_CTX_cleanup(ctx);
    EVP_CIPHER_CTX_free(ctx);
}

//--------------------------------------------------------------------------------------------------
void EVP_PKEY_CTX_Deleter::operator()(evp_pkey_ctx_st* ctx)
{
    EVP_PKEY_CTX_free(ctx);
}

//--------------------------------------------------------------------------------------------------
void EVP_PKEY_Deleter::operator()(evp_pkey_st* pkey)
{
    EVP_PKEY_free(pkey);
}

namespace {

//--------------------------------------------------------------------------------------------------
const EVP_CIPHER* cipherType(CipherType type)
{
    switch (type)
    {
        case CipherType::AES256_GCM:
            return EVP_aes_256_gcm();

        case CipherType::CHACHA20_POLY1305:
            return EVP_chacha20_poly1305();

        default:
            NOTREACHED();
            return nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
int cipherMode(CipherMode mode)
{
    switch (mode)
    {
        case CipherMode::ENCRYPT:
            return 1;

        case CipherMode::DECRYPT:
            return 0;

        default:
            NOTREACHED();
            return -1;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
EVP_CIPHER_CTX_ptr createCipher(CipherType type, CipherMode mode, const QByteArray& key, int iv_size)
{
    EVP_CIPHER_CTX_ptr ctx(EVP_CIPHER_CTX_new());
    if (!ctx)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_new failed";
        return nullptr;
    }

    if (EVP_CipherInit_ex(ctx.get(), cipherType(type), nullptr, nullptr, nullptr,
                          cipherMode(mode)) != 1)
    {
        LOG(ERROR) << "EVP_EncryptInit_ex failed";
        return nullptr;
    }

    if (EVP_CIPHER_CTX_set_key_length(ctx.get(), static_cast<int>(key.size())) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_set_key_length failed";
        return nullptr;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, iv_size, nullptr) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return nullptr;
    }

    if (EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, reinterpret_cast<const quint8*>(key.data()),
                          nullptr, cipherMode(mode)) != 1)
    {
        LOG(ERROR) << "EVP_CIPHER_CTX_ctrl failed";
        return nullptr;
    }

    return ctx;
}

} // namespace base
