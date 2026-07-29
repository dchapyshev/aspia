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

#ifndef BASE_STRING_UTIL_H
#define BASE_STRING_UTIL_H

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

// Concatenates all |parts| into one string with a single allocation: sums the total length,
// reserves it once, then appends each part. Cheaper than chained operator+ / append(), which
// reallocate repeatedly as the string grows.
std::string strCat(std::span<const std::string_view> parts);

// Convenience overload for a braced list of parts: strCat({"a", b, "c"}).
std::string strCat(std::initializer_list<std::string_view> parts);

// Concatenates all |parts| into one string with |separator| between them, with a single
// allocation. An empty part keeps its place, the same as any other.
std::string strJoin(std::span<const std::string> parts, std::string_view separator);

// Returns |str| with leading and trailing ASCII whitespace stripped. The result is a view into
// |str| (no allocation) and stays valid only while |str|'s buffer lives. Bytes above 0x7F are
// left untouched, so multi-byte UTF-8 sequences pass through unharmed.
std::string_view strTrimmed(std::string_view str);

// Case of the letters in a hexadecimal number.
enum class HexCase { UPPER, LOWER };

// Returns |value| in hexadecimal, padded with zeros to |digits|. A value too large for the width
// is written in full: the width is the minimum the value is shown in, not a limit.
std::string strHex(uint64_t value, int digits, HexCase hex_case = HexCase::UPPER);

#endif // BASE_STRING_UTIL_H
