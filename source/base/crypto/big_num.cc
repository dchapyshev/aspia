//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/crypto/big_num.h"

#include <openssl/opensslv.h>
#include <openssl/bn.h>

namespace base {

//--------------------------------------------------------------------------------------------------
BigNum::BigNum(const quint8* buffer, size_t buffer_size)
{
    if (!buffer || !buffer_size)
        return;

    num_.reset(BN_bin2bn(buffer, static_cast<int>(buffer_size), nullptr));
}

//--------------------------------------------------------------------------------------------------
BigNum::BigNum(BigNum&& other) noexcept
{
    reset(other.release());
}

//--------------------------------------------------------------------------------------------------
BigNum& BigNum::operator=(BigNum&& other) noexcept
{
    reset(other.release());
    return *this;
}

//--------------------------------------------------------------------------------------------------
BigNum::~BigNum() = default;

//--------------------------------------------------------------------------------------------------
void BigNum::reset(bignum_st* num)
{
    num_.reset(num);
}

//--------------------------------------------------------------------------------------------------
bignum_st* BigNum::release()
{
    return num_.release();
}

//--------------------------------------------------------------------------------------------------
std::string BigNum::toStdString() const
{
    if (!isValid())
        return std::string();

    int length = BN_num_bytes(num_.get());
    if (length <= 0)
        return std::string();

    std::string result;
    result.resize(static_cast<std::string::size_type>(length));

    BN_bn2bin(num_.get(), reinterpret_cast<quint8*>(result.data()));
    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray BigNum::toByteArray() const
{
    if (!isValid())
        return QByteArray();

    int length = BN_num_bytes(num_.get());
    if (length <= 0)
        return QByteArray();

    QByteArray result;
    result.resize(static_cast<std::string::size_type>(length));

    BN_bn2bin(num_.get(), reinterpret_cast<quint8*>(result.data()));
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
BigNum BigNum::create()
{
    return BigNum(BN_new());
}

//--------------------------------------------------------------------------------------------------
// static
BigNum BigNum::fromStdString(std::string_view string)
{
    return BigNum(reinterpret_cast<const quint8*>(string.data()), string.size());
}

//--------------------------------------------------------------------------------------------------
// static
BigNum BigNum::fromByteArray(const QByteArray& array)
{
    return BigNum(reinterpret_cast<const quint8*>(array.data()), array.size());
}

//--------------------------------------------------------------------------------------------------
BigNum::Context::Context(Context&& other) noexcept
{
    reset(other.release());
}

//--------------------------------------------------------------------------------------------------
BigNum::Context& BigNum::Context::operator=(Context&& other) noexcept
{
    reset(other.release());
    return *this;
}

//--------------------------------------------------------------------------------------------------
BigNum::Context::~Context() = default;

//--------------------------------------------------------------------------------------------------
// static
BigNum::Context BigNum::Context::create()
{
    return Context(BN_CTX_new());
}

//--------------------------------------------------------------------------------------------------
void BigNum::Context::reset(bignum_ctx* ctx)
{
    ctx_.reset(ctx);
}

//--------------------------------------------------------------------------------------------------
bignum_ctx* BigNum::Context::release()
{
    return ctx_.release();
}

} // namespace base
