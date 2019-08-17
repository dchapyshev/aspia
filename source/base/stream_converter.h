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

#ifndef BASE__STREAM_CONVERTER_H
#define BASE__STREAM_CONVERTER_H

#include <optional>
#include <sstream>
#include <string>

namespace base {

template <typename ValueType>
class StreamConverter
{
    template <typename ValueType>
    struct ConverterImpl
    {
        static void insert(std::ostringstream& stream, const ValueType& value)
        {
            stream << value;
        }

        static void extract(std::istringstream& stream, ValueType& value)
        {
            stream >> value;

            if (!stream.eof())
                stream >> std::ws;
        }
    };

    template <>
    struct ConverterImpl<bool>
    {
        static void insert(std::ostringstream& stream, bool value)
        {
            stream.setf(std::ios_base::boolalpha);
            stream << value;
        }

        static void extract(std::istringstream& stream, bool& value)
        {
            stream >> value;

            if (stream.fail())
            {
                stream.clear();
                stream.setf(std::ios_base::boolalpha);
                stream >> value;
            }

            if (!stream.eof())
                stream >> std::ws;
        }
    };

    template <>
    struct ConverterImpl<char>
    {
        static void insert(std::ostringstream& stream, char value)
        {
            stream << value;
        }

        static void extract(std::istringstream& stream, char& value)
        {
            stream.unsetf(std::ios_base::skipws);
            stream >> value;
        }
    };

    using Converter = ConverterImpl<ValueType>;

public:
    static std::optional<ValueType> get_value(const std::string& str)
    {
        std::istringstream stream(str);
        stream.imbue(std::locale::classic());

        ValueType result;
        Converter::extract(stream, result);

        if (stream.fail() || stream.bad())
            return std::optional<ValueType>();

        if (stream.get() != std::istringstream::traits_type::eof())
            return std::optional<ValueType>();

        return result;
    }

    static std::optional<std::string> set_value(const ValueType& value)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());

        Converter::insert(stream, value);
        if (stream)
            return stream.str();

        return std::optional<std::string>();
    }
};

} // namespace base

#endif // BASE__STREAM_CONVERTER_H
