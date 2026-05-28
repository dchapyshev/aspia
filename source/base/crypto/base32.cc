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

#include "base/crypto/base32.h"

namespace {

const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// Number of Base32 characters produced by the final group, indexed by |input_size % 5|.
const qsizetype kTailChars[5] = { 0, 2, 4, 5, 7 };

//--------------------------------------------------------------------------------------------------
// Maps a Base32 character to its 5-bit value, or 0xFF for invalid input. Accepts both upper-
// and lower-case letters per RFC 4648 section 12.
quint8 decodeChar(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return static_cast<quint8>(ch - 'A');
    if (ch >= 'a' && ch <= 'z')
        return static_cast<quint8>(ch - 'a');
    if (ch >= '2' && ch <= '7')
        return static_cast<quint8>(ch - '2' + 26);
    return 0xFF;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QByteArray Base32::encode(const QByteArray& data, bool with_padding)
{
    if (data.isEmpty())
        return QByteArray();

    const qsizetype input_size = data.size();
    const qsizetype full_groups = input_size / 5;
    const qsizetype remainder = input_size % 5;
    const qsizetype unpadded_chars = full_groups * 8 + kTailChars[remainder];
    const qsizetype output_size = with_padding ? (full_groups + (remainder ? 1 : 0)) * 8 : unpadded_chars;

    QByteArray out;
    out.resize(output_size);
    char* dst = out.data();
    const quint8* src = reinterpret_cast<const quint8*>(data.constData());

    auto encode_group = [&](const quint8* in, int in_len, char* o)
    {
        quint64 buffer = 0;
        for (int i = 0; i < in_len; ++i)
            buffer = (buffer << 8) | in[i];
        // Shift so that the highest 5 bits of the resulting 40-bit word are aligned with the
        // first output character, regardless of |in_len|.
        buffer <<= (5 - in_len) * 8;
        for (int i = 0; i < 8; ++i)
            o[i] = kAlphabet[(buffer >> (35 - 5 * i)) & 0x1F];
    };

    qsizetype written = 0;
    for (qsizetype i = 0; i < full_groups; ++i)
    {
        encode_group(src + i * 5, 5, dst + written);
        written += 8;
    }

    if (remainder != 0)
    {
        char group[8];
        encode_group(src + full_groups * 5, static_cast<int>(remainder), group);
        const int chars_in_group = unpadded_chars - full_groups * 8;
        for (int i = 0; i < chars_in_group; ++i)
            dst[written + i] = group[i];
        if (with_padding)
        {
            for (int i = chars_in_group; i < 8; ++i)
                dst[written + i] = '=';
            written += 8;
        }
        else
        {
            written += chars_in_group;
        }
    }

    out.resize(written);
    return out;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray Base32::decode(const QByteArray& data, bool* ok)
{
    auto fail = [ok]()
    {
        if (ok)
            *ok = false;
        return QByteArray();
    };

    if (ok)
        *ok = true;
    if (data.isEmpty())
        return QByteArray();

    // Strip optional padding ('=') and any trailing whitespace.
    qsizetype length = data.size();
    while (length > 0 && (data[length - 1] == '=' || data[length - 1] == '\n' ||
                          data[length - 1] == '\r' || data[length - 1] == ' '))
    {
        --length;
    }

    if (length == 0)
        return QByteArray();

    // Per RFC 4648 section 6 only 2, 4, 5, 7 or 8 unpadded characters per final group are legal.
    const qsizetype tail = length % 8;
    if (tail == 1 || tail == 3 || tail == 6)
        return fail();

    QByteArray out;
    out.reserve(length * 5 / 8);

    quint64 buffer = 0;
    int bits = 0;
    for (qsizetype i = 0; i < length; ++i)
    {
        const quint8 v = decodeChar(data[i]);
        if (v == 0xFF)
            return fail();
        buffer = (buffer << 5) | v;
        bits += 5;
        if (bits >= 8)
        {
            bits -= 8;
            out.append(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }

    // Any leftover bits must be zero - otherwise the input is malformed.
    if (bits > 0 && (buffer & ((quint64(1) << bits) - 1)) != 0)
        return fail();

    return out;
}
