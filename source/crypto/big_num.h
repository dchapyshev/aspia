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

#ifndef CRYPTO__BIG_NUM_H
#define CRYPTO__BIG_NUM_H

#include "base/byte_array.h"
#include "base/const_buffer.h"
#include "base/macros_magic.h"
#include "crypto/openssl_util.h"

#include <memory>

namespace crypto {

class BigNum
{
public:
    BigNum() = default;

    BigNum(BigNum&& other) noexcept;
    BigNum& operator=(BigNum&& other) noexcept;

    ~BigNum();

    bool isValid() const { return num_ != nullptr; }

    void reset(bignum_st* num = nullptr);
    bignum_st* release();

    bignum_st* get() { return num_.get(); }
    const bignum_st* get() const { return num_.get(); }

    std::string toStdString() const;
    base::ByteArray toByteArray() const;

    static BigNum create();
    static BigNum fromBuffer(const base::ConstBuffer& buffer);
    static BigNum fromStdString(std::string_view string);
    static BigNum fromByteArray(const base::ByteArray& array);

    operator bignum_st*() const { return num_.get(); }

    class Context
    {
    public:
        Context(Context&& other) noexcept;
        Context& operator=(Context&& other) noexcept;

        ~Context();

        static Context create();

        bool isValid() const { return ctx_ != nullptr; }

        void reset(bignum_ctx* ctx = nullptr);
        bignum_ctx* release();

        bignum_ctx* get() { return ctx_.get(); }
        const bignum_ctx* get() const { return ctx_.get(); }

        operator bignum_ctx*() const { return ctx_.get(); }

    private:
        Context(bignum_ctx* ctx)
            : ctx_(ctx)
        {
            // Nothing
        }

        BIGNUM_CTX_ptr ctx_;
        DISALLOW_COPY_AND_ASSIGN(Context);
    };

private:
    explicit BigNum(bignum_st* num)
        : num_(num)
    {
        // Nothing
    }

    BigNum(const uint8_t* buffer, size_t buffer_size);

    BIGNUM_ptr num_;

    DISALLOW_COPY_AND_ASSIGN(BigNum);
};

} // namespace crypto

#endif // CRYPTO__BIG_NUM_H
