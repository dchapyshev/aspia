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
// This file based on MODP_B64 - High performance base64 encoder/decoder (BSD License).
// Copyright (C) 2005, 2006  Nick Galbreath - nickg [at] modp [dot] com
//

#include "base/base64.h"

#include "base/base64_constants.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace base {

namespace {

const uint32_t kBadChar = 0x01FFFFFF;
const char kPaddingChar = '=';

} // namespace

// static
void Base64::encode(const std::string& input, std::string* output)
{
    DCHECK(output);
    output->swap(encode(input));
}

// static
std::string Base64::encode(const std::string& input)
{
    return encodeT<std::string, std::string>(input);
}

// static
bool Base64::decode(const std::string& input, std::string* output)
{
    return decodeT<std::string>(input, output);
}

// static
std::string Base64::decode(const std::string& input)
{
    return decodeT<std::string, std::string>(input);
}

// static
size_t Base64::encodeImpl(char* dest, const char* str, size_t len)
{
    uint8_t* p = reinterpret_cast<uint8_t*>(dest);
    size_t i = 0;

    // Unsigned here is important!
    uint8_t t1, t2, t3;

    if (len > 2)
    {
        for (; i < len - 2; i += 3)
        {
            t1 = str[i];
            t2 = str[i + 1];
            t3 = str[i + 2];

            *p++ = kE0[t1];
            *p++ = kE1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *p++ = kE1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
            *p++ = kE2[t3];
        }
    }

    switch (len - i)
    {
        case 0:
            break;

        case 1:
            t1 = str[i];
            *p++ = kE0[t1];
            *p++ = kE1[(t1 & 0x03) << 4];
            *p++ = kPaddingChar;
            *p++ = kPaddingChar;
            break;

        default: // case 2
            t1 = str[i];
            t2 = str[i + 1];

            *p++ = kE0[t1];
            *p++ = kE1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *p++ = kE2[(t2 & 0x0F) << 2];
            *p++ = kPaddingChar;
    }

    *p = '\0';
    return p - reinterpret_cast<uint8_t*>(dest);
}

#if (ARCH_CPU_BIG_ENDIAN == 1)
// static
int Base64::decodeImpl(char* dest, const char* src, int len)
{
    if (len == 0)
        return 0;

    // If padding is used, then the message must be at least 4 chars and be a multiple of 4.
    // There can be at most 2 pad chars at the end.
    if (len < 4 || (len % 4 != 0))
        return kErrorResult;

    if (src[len - 1] == kPaddingChar)
    {
        --len;
        if (src[len - 1] == kPaddingChar)
            --len;
    }

    size_t i;
    int leftover = len % 4;
    size_t chunks = (leftover == 0) ? len / 4 - 1 : len / 4;

    uint8_t * p = reinterpret_cast<uint8_t*>(dest);
    uint32_t x = 0;
    uint32_t * dest_int = reinterpret_cast<uint32_t*>(p);
    const uint32_t * src_int = reinterpret_cast<const uint32_t*>(src);
    uint32_t y = *src_int++;

    for (i = 0; i < chunks; ++i)
    {
        x = kD0[y >> 24 & 0xff] | kD1[y >> 16 & 0xff] | kD2[y >> 8 & 0xff] | kD3[y & 0xff];

        if (x >= kBadChar)
            return kErrorResult;

        *dest_int = x << 8;
        p += 3;
        dest_int = reinterpret_cast<uint32_t*>(p);
        y = *src_int++;
    }

    switch (leftover)
    {
        case 0:
        {
            x = kD0[y >> 24 & 0xff] | kD1[y >> 16 & 0xff] | kD2[y >> 8 & 0xff] | kD3[y & 0xff];
            if (x >= kBadChar)
                return kErrorResult;

            *p++ = reinterpret_cast<uint8_t*>(&x)[1];
            *p++ = reinterpret_cast<uint8_t*>(&x)[2];
            *p = reinterpret_cast<uint8_t*>(&x)[3];
            return (chunks + 1) * 3;
        }
        break;

        case 1:
        {
            x = kD3[y >> 24];
            *p = static_cast<uint8_t>(x);
        }
        break;

        case 2:
        {
            x = kD3[y >> 24] * 64 + kD3[(y >> 16) & 0xff];
            *p = static_cast<uint8_t>(x >> 4);
        }
        break;

        default:  /* case 3 */
        {
            x = (kD3[y >> 24] * 64 + kD3[(y >> 16) & 0xff]) * 64 + kD3[(y >> 8) & 0xff];
            *p++ = static_cast<uint8_t>(x >> 10);
            *p = static_cast<uint8_t>(x >> 2);
        }
        break;
    }

    if (x >= kBadChar)
        return kErrorResult;

    return 3 * chunks + (6 * leftover) / 8;
}
#elif (ARCH_CPU_LITTLE_ENDIAN == 1)
// static
size_t Base64::decodeImpl(char* dest, const char* src, size_t len)
{
    if (!len)
        return 0;

    // If padding is used, then the message must be at least 4 chars and be a multiple of 4.
    if (len < 4 || (len % 4 != 0))
        return kErrorResult;

    // There can be at most 2 pad chars at the end.
    if (src[len - 1] == kPaddingChar)
    {
        --len;
        if (src[len - 1] == kPaddingChar)
            --len;
    }

    size_t i;
    int leftover = len % 4;
    size_t chunks = (leftover == 0) ? len / 4 - 1 : len / 4;

    uint8_t * p = reinterpret_cast<uint8_t*>(dest);
    uint32_t x = 0;
    const uint8_t * y = reinterpret_cast<const uint8_t*>(src);

    for (i = 0; i < chunks; ++i, y += 4)
    {
        x = kD0[y[0]] | kD1[y[1]] | kD2[y[2]] | kD3[y[3]];
        if (x >= kBadChar)
            return kErrorResult;

        *p++ = reinterpret_cast<uint8_t*>(&x)[0];
        *p++ = reinterpret_cast<uint8_t*>(&x)[1];
        *p++ = reinterpret_cast<uint8_t*>(&x)[2];
    }

    switch (leftover)
    {
        case 0:
        {
            x = kD0[y[0]] | kD1[y[1]] | kD2[y[2]] | kD3[y[3]];
            if (x >= kBadChar)
                return kErrorResult;

            *p++ = reinterpret_cast<uint8_t*>(&x)[0];
            *p++ = reinterpret_cast<uint8_t*>(&x)[1];
            *p = reinterpret_cast<uint8_t*>(&x)[2];
            return (chunks + 1) * 3;
        }
        break;

        case 1: // With padding this is an impossible case.
        {
            x = kD0[y[0]];
            *p = *reinterpret_cast<uint8_t*>(&x); // i.e. first char/byte in int
        }
        break;

        case 2: // Case 2, 1 output byte.
        {
            x = kD0[y[0]] | kD1[y[1]];
            *p = *reinterpret_cast<uint8_t*>(&x); // i.e. first char
        }
        break;

        default: // Case 3, 2 output bytes.
        {
            x = kD0[y[0]] | kD1[y[1]] | kD2[y[2]]; // 0x3c
            *p++ = reinterpret_cast<uint8_t*>(&x)[0];
            *p = reinterpret_cast<uint8_t*>(&x)[1];
        }
        break;
    }

    if (x >= kBadChar)
        return kErrorResult;

    return 3 * chunks + (6 * leftover) / 8;
}
#else
#error CPU endian not specified
#endif

// static
size_t Base64::encodedLength(size_t len)
{
    return ((len + 2) / 3 * 4 + 1);
}

// static
size_t Base64::decodedLength(size_t len)
{
    return (len / 4 * 3 + 2);
}

} // namespace base
