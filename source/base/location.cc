//
// Aspia Project
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

#include "base/compiler_specific.h"

namespace base {

//--------------------------------------------------------------------------------------------------
Location::Location() = default;

//--------------------------------------------------------------------------------------------------
Location::Location(const Location& other) = default;

//--------------------------------------------------------------------------------------------------
Location& Location::operator=(const Location& other) = default;

//--------------------------------------------------------------------------------------------------
Location::Location(const char* file_name)
    : file_name_(file_name)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Location::Location(const char* function_name, const char* file_name, int line_number)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString Location::toString(PathType path_type) const
{
    std::string_view file_name(file_name_);

    if (path_type == SHORT_PATH)
    {
        size_t last_slash_pos = file_name.find_last_of("\\/");
        if (last_slash_pos != std::string_view::npos)
            file_name.remove_prefix(last_slash_pos + 1);
    }

    return QString(function_name_) + "@" + file_name.data() + ":" + QString::number(line_number_);
}

//--------------------------------------------------------------------------------------------------
// static
NOINLINE Location Location::createFromHere(const char* file_name)
{
    return Location(file_name);
}

//--------------------------------------------------------------------------------------------------
// static
NOINLINE Location Location::createFromHere(const char* function_name,
                                           const char* file_name,
                                           int line_number)
{
    return Location(function_name, file_name, line_number);
}

} // namespace base
