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

#include "crypto/openssl_util.h"

#include <openssl/bn.h>
#include <openssl/evp.h>

namespace aspia {

void BIGNUM_CTX_Deleter::operator()(bignum_ctx* bignum_ctx)
{
    BN_CTX_free(bignum_ctx);
}

void BIGNUM_Deleter::operator()(bignum_st* bignum)
{
    BN_clear_free(bignum);
}

void EVP_CIPHER_CTX_Deleter::operator()(evp_cipher_ctx_st* ctx)
{
    EVP_CIPHER_CTX_cleanup(ctx);
    EVP_CIPHER_CTX_free(ctx);
}

} // namespace aspia
