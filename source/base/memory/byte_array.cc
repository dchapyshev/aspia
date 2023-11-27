//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/memory/byte_array.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
int charToHex(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';

    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;

    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;

    return -1;
}

//--------------------------------------------------------------------------------------------------
char hexToChar(int value)
{
    return "0123456789ABCDEF"[value & 0xF];
}

} // namespace

//--------------------------------------------------------------------------------------------------
ByteArray fromData(const void* data, size_t size)
{
    if (!data || !size)
        return ByteArray();

    ByteArray out;
    out.resize(size);

    memcpy(out.data(), data, size);
    return out;
}

//--------------------------------------------------------------------------------------------------
ByteArray fromStdString(std::string_view in)
{
    return fromData(in.data(), in.size());
}

//--------------------------------------------------------------------------------------------------
std::string toStdString(const ByteArray& in)
{
    if (in.empty())
        return std::string();

    std::string out;
    out.resize(in.size());

    memcpy(out.data(), in.data(), in.size());
    return out;
}

//--------------------------------------------------------------------------------------------------
ByteArray fromHex(std::string_view in)
{
    if (in.empty())
        return ByteArray();

    ByteArray out;
    out.resize((in.size() + 1) / 2);

    ByteArray::iterator result = out.end();

    bool is_odd = true;
    for (auto it = in.crbegin(); it != in.crend(); ++it)
    {
        const int hex = charToHex(*it);
        if (hex == -1)
            continue;

        if (is_odd)
            *--result = static_cast<uint8_t>(hex);
        else
            *result |= hex << 4;

        is_odd = !is_odd;
    }

    out.erase(out.begin(), result);
    return out;
}

//--------------------------------------------------------------------------------------------------
std::string toHex(const ByteArray& in)
{
    if (in.empty())
        return std::string();

    std::string out;
    out.resize(in.size() * 2);

    char *dst = out.data();

    for (const auto& hex : in)
    {
        *dst++ = hexToChar(hex >> 4);
        *dst++ = hexToChar(hex & 0xF);
    }

    return out;
}

//--------------------------------------------------------------------------------------------------
void append(ByteArray* in, const void* data, size_t size)
{
    if (!in || !data || !size)
        return;

    size_t initial_size = in->size();
    in->resize(initial_size + size);
    memcpy(in->data() + initial_size, data, size);
}

//--------------------------------------------------------------------------------------------------
base::ByteArray serialize(const google::protobuf::MessageLite& message)
{
    const size_t size = message.ByteSizeLong();
    if (!size)
        return base::ByteArray();

    base::ByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(buffer.data());
    return buffer;
}

//--------------------------------------------------------------------------------------------------
int compare(const base::ByteArray& first, const base::ByteArray& second)
{
    if (first.empty() && second.empty())
        return 0;

    if (first.size() < second.size())
        return -1;

    if (first.size() > second.size())
        return 1;

    return memcmp(first.data(), second.data(), first.size());
}

//--------------------------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const ByteArray& bytearray)
{
    return out << toHex(bytearray);
}

} // namespace base
