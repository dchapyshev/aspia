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

#include "base/memory/byte_array.h"

#include "base/logging.h"

#include <algorithm>
#include <sstream>

namespace base {

namespace {

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

} // namespace

ByteArray fromData(const void* data, size_t size)
{
    if (!data || !size)
        return ByteArray();

    ByteArray out;
    out.resize(size);

    memcpy(out.data(), data, size);
    return out;
}

ByteArray fromStdString(std::string_view in)
{
    return fromData(in.data(), in.size());
}

std::string toStdString(const ByteArray& in)
{
    if (in.empty())
        return std::string();

    std::string out;
    out.resize(in.size());

    memcpy(out.data(), in.data(), in.size());
    return out;
}

ByteArray fromHex(std::string_view in)
{
    if (in.empty())
        return ByteArray();

    bool high_part = (in.size() % 2) == 0;

    ByteArray out;

    for (size_t i = 0; i < in.size(); ++i)
    {
        int hex = charToHex(in[i]);
        if (hex == -1)
            return ByteArray();

        if (high_part)
            out.push_back(0x10 * hex);
        else
            out.back() += hex;

        high_part = !high_part;
    }

    return out;
}

std::string toHex(const ByteArray& in)
{
    if (in.empty())
        return std::string();

    std::stringstream stream;

    for (size_t i = 0; i < in.size(); ++i)
        stream << std::hex << in[i];

    return stream.str();
}

std::ostream& operator<<(std::ostream& out, const ByteArray& bytearray)
{
    return out << toHex(bytearray);
}

} // namespace base
