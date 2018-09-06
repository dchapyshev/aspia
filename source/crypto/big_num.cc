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

#include "crypto/big_num.h"

#include <openssl/opensslv.h>
#include <openssl/bn.h>

namespace aspia {

BigNum::BigNum(const ConstBuffer& buffer)
{
    if (!buffer.isValid())
        return;

    num_ = BN_bin2bn(buffer.data(), buffer.size(), nullptr);
}

BigNum::BigNum(const std::string& string)
{
    if (string.empty())
        return;

    num_ = BN_bin2bn(reinterpret_cast<const uint8_t*>(string.data()), string.size(), nullptr);
}

BigNum::BigNum(BigNum&& other) noexcept
{
    reset(other.release());
}

BigNum& BigNum::operator=(BigNum&& other) noexcept
{
    reset(other.release());
    return *this;
}

BigNum::~BigNum()
{
    reset();
}

void BigNum::reset(bignum_st* num)
{
    if (isValid())
        BN_clear_free(num_);

    num_ = num;
}

bignum_st* BigNum::release()
{
    bignum_st* tmp = num_;
    num_ = nullptr;
    return tmp;
}

std::string BigNum::toStdString() const
{
    if (!isValid())
        return std::string();

    int length = BN_num_bytes(num_);
    if (length <= 0)
        return std::string();

    std::string result;
    result.resize(length);

    BN_bn2bin(num_, reinterpret_cast<uint8_t*>(result.data()));
    return result;
}

// static
BigNum BigNum::create()
{
    return BigNum(BN_new());
}

// static
BigNum BigNum::fromBuffer(const ConstBuffer& buffer)
{
    return BigNum(buffer);
}

// static
BigNum BigNum::fromStdString(const std::string& string)
{
    return BigNum(string);
}

BigNum::Context::Context(Context&& other) noexcept
{
    reset(other.release());
}

BigNum::Context& BigNum::Context::operator=(Context&& other) noexcept
{
    reset(other.release());
}

BigNum::Context::~Context()
{
    reset();
}

// static
BigNum::Context BigNum::Context::create()
{
    return Context(BN_CTX_new());
}

void BigNum::Context::reset(bignum_ctx* ctx)
{
    if (isValid())
        BN_CTX_free(ctx_);

    ctx_ = ctx;
}

bignum_ctx* BigNum::Context::release()
{
    bignum_ctx* tmp = ctx_;
    ctx_ = nullptr;
    return tmp;
}

} // namespace aspia
