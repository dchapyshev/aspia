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

#include "base/strings/string_number_conversions.h"

#include "base/logging.h"

#include <charconv>
#include <limits>

namespace base {

namespace {

template <typename NumberType>
bool stringToNumberT(std::string_view input, NumberType* output)
{
    DCHECK(output);

    NumberType number;

    std::from_chars_result result =
        std::from_chars(input.data(), input.data() + input.size(), number);

    if (result.ec != std::errc())
        return false;

    if (result.ptr != input.data() + input.size())
        return false;

    *output = number;
    return true;
}

template <typename NumberType>
std::string numberToStringT(NumberType value)
{
    size_t digits_count = std::numeric_limits<NumberType>::digits10 + 1;

    if (std::numeric_limits<NumberType>::is_signed)
        ++digits_count;

    std::string ret;
    ret.resize(digits_count);

    std::to_chars_result result = std::to_chars(ret.data(), ret.data() + ret.size(), value);
    if (result.ec != std::errc())
        return std::string();

    ret.resize(result.ptr - ret.data());
    return ret;
}

} // namespace

bool stringToInt(std::string_view input, int* output)
{
    return stringToNumberT<int>(input, output);
}

bool stringToUint(std::string_view input, unsigned* output)
{
    return stringToNumberT<unsigned>(input, output);
}

bool stringToULong(std::string_view input, unsigned long* output)
{
    return stringToNumberT<unsigned long>(input, output);
}

bool stringToInt64(std::string_view input, int64_t* output)
{
    return stringToNumberT<int64_t>(input, output);
}

bool stringToUint64(std::string_view input, uint64_t* output)
{
    return stringToNumberT<uint64_t>(input, output);
}

bool stringToSizeT(std::string_view input, size_t* output)
{
    return stringToNumberT<size_t>(input, output);
}

std::string numberToString(int value)
{
    return numberToStringT(value);
}

std::string numberToString(unsigned int value)
{
    return numberToStringT(value);
}

std::string numberToString(long value)
{
    return numberToStringT(value);
}

std::string numberToString(unsigned long value)
{
    return numberToStringT(value);
}

std::string numberToString(long long value)
{
    return numberToStringT(value);
}

std::string numberToString(unsigned long long value)
{
    return numberToStringT(value);
}

} // namespace base
