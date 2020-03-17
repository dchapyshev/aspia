//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CONVERTER_H
#define BASE__CONVERTER_H

#include "base/base64.h"
#include "base/memory/byte_array.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

#include <optional>
#include <string>

namespace base {

namespace internal {

template <typename T>
struct ConverterImpl
{
    // Nothing
};

template <>
struct ConverterImpl<std::string>
{
    static bool fromString(std::string_view str, std::string* value)
    {
        value->assign(str);
        return true;
    }

    static std::string toString(const std::string& value)
    {
        return value;
    }
};

template <>
struct ConverterImpl<std::u16string>
{
    static bool fromString(std::string_view str, std::u16string* value)
    {
        return utf8ToUtf16(str, value);
    }

    static std::string toString(const std::u16string& value)
    {
        return utf8FromUtf16(value);
    }
};

template <>
struct ConverterImpl<ByteArray>
{
    static bool fromString(std::string_view str, ByteArray* value)
    {
        return base::Base64::decodeT<std::string_view, base::ByteArray>(str, value);
    }

    static std::string toString(const ByteArray& value)
    {
        return base::Base64::encodeT<base::ByteArray, std::string>(value);
    }
};

template <>
struct ConverterImpl<int>
{
    static bool fromString(std::string_view str, int* value)
    {
        return stringToInt(str, value);
    }

    static std::string toString(int value)
    {
        return numberToString(value);
    }
};

template <>
struct ConverterImpl<size_t>
{
    static bool fromString(std::string_view str, size_t* value)
    {
        return stringToSizeT(str, value);
    }

    static std::string toString(size_t value)
    {
        return numberToString(value);
    }
};

template <>
struct ConverterImpl<uint16_t>
{
    static bool fromString(std::string_view str, uint16_t* value)
    {
        return stringToUShort(str, value);
    }

    static std::string toString(size_t value)
    {
        return numberToString(value);
    }
};

template <>
struct ConverterImpl<bool>
{
    static bool fromString(std::string_view str, bool* value)
    {
        if (str == "true" || str == "1")
            *value = true;
        else if (str == "false" || str == "0")
            *value = false;
        else
            return false;

        return true;
    }

    static std::string toString(bool value)
    {
        return value ? "true" : "false";
    }
};

} // namespace internal

template <typename ValueType>
class Converter
{
public:
    static std::optional<ValueType> get_value(std::string_view str)
    {
        std::string_view temp = trimWhitespaceASCII(str, TRIM_ALL);
        if (temp.empty())
            return std::nullopt;

        ValueType result;
        if (!internal::ConverterImpl<ValueType>::fromString(temp, &result))
            return std::nullopt;

        return result;
    }

    static std::string set_value(const ValueType& value)
    {
        return internal::ConverterImpl<ValueType>::toString(value);
    }
};

} // namespace base

#endif // BASE__CONVERTER_H
