//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/location.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Location::Location() = default;

//--------------------------------------------------------------------------------------------------
Location::Location(const Location& other) = default;

//--------------------------------------------------------------------------------------------------
Location& Location::operator=(const Location& other) = default;

//--------------------------------------------------------------------------------------------------
Location::Location(const char* function_name, const char* file_name, int line_number)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString Location::toString() const
{
    std::string_view file_name(file_name_);

    size_t last_slash_pos = file_name.find_last_of("\\/");
    if (last_slash_pos != std::string_view::npos)
        file_name.remove_prefix(last_slash_pos + 1);

    return QString(function_name_) + "@" + file_name.data() + ":" + QString::number(line_number_);
}

//--------------------------------------------------------------------------------------------------
// static
Q_NEVER_INLINE Location Location::createFromHere(const char* function_name,
                                                 const char* file_name,
                                                 int line_number)
{
    return Location(function_name, file_name, line_number);
}

} // namespace base

//--------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug out, const base::Location& location)
{
    return out << location.toString();
}
