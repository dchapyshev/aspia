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

#include "base/string_util.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool isAsciiWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r';
}

} // namespace

//--------------------------------------------------------------------------------------------------
std::string strCat(std::span<const std::string_view> parts)
{
    size_t total = 0;
    for (std::string_view part : parts)
        total += part.size();

    std::string result;
    result.reserve(total);

    for (std::string_view part : parts)
        result.append(part);

    return result;
}

//--------------------------------------------------------------------------------------------------
std::string strCat(std::initializer_list<std::string_view> parts)
{
    return strCat(std::span<const std::string_view>(parts.begin(), parts.size()));
}

//--------------------------------------------------------------------------------------------------
std::string strJoin(std::span<const std::string> parts, std::string_view separator)
{
    size_t total = 0;
    for (const std::string& part : parts)
        total += part.size();

    if (!parts.empty())
        total += separator.size() * (parts.size() - 1);

    std::string result;
    result.reserve(total);

    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
            result.append(separator);

        result.append(parts[i]);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
std::string_view strTrimmed(std::string_view str)
{
    size_t begin = 0;
    size_t end = str.size();

    while (begin < end && isAsciiWhitespace(str[begin]))
        ++begin;

    while (end > begin && isAsciiWhitespace(str[end - 1]))
        --end;

    return str.substr(begin, end - begin);
}

//--------------------------------------------------------------------------------------------------
std::string strHex(uint64_t value, int digits, HexCase hex_case)
{
    const char* alphabet = hex_case == HexCase::UPPER ? "0123456789ABCDEF" : "0123456789abcdef";
    char buffer[16];
    int length = 0;

    do
    {
        buffer[length++] = alphabet[value & 0x0F];
        value >>= 4;
    }
    while (value);

    std::string result;

    if (digits > length)
        result.append(static_cast<size_t>(digits - length), '0');

    while (length > 0)
        result += buffer[--length];

    return result;
}
