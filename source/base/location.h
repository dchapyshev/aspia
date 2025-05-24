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

#ifndef BASE_LOCATION_H
#define BASE_LOCATION_H

#include <QString>

#include "build/build_config.h"

namespace base {

// Location provides basic info where of an object was constructed, or was significantly brought
// to life.
class Location
{
public:
    Location();

    Location(const Location& other);
    Location& operator=(const Location& other);

    // Only initializes the file name and program counter, the source information will be null for
    // the strings, and -1 for the line number.
    explicit Location(const char* file_name);

    // Constructor should be called with a long-lived char*, such as __FILE__. It assumes the
    // provided value will persist as a global constant, and it will not make a copy of it.
    Location(const char* function_name, const char* file_name, int line_number);

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* functionName() const { return function_name_; }

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* fileName() const { return file_name_; }

    // Will be -1 for default initialized Location objects and when source names are disabled.
    int lineNumber() const { return line_number_; }

    enum PathType
    {
        FULL_PATH,
        SHORT_PATH
    };

    // Converts to the most user-readable form possible. If function and filename
    // are not available, this will return "pc:<hex address>".
    QString toString(PathType path_type = SHORT_PATH) const;

    static Location createFromHere(const char* file_name);
    static Location createFromHere(const char* function_name,
                                   const char* file_name,
                                   int line_number);

private:
    const char* function_name_ = nullptr;
    const char* file_name_ = nullptr;
    int line_number_ = -1;
};

#if defined(ENABLE_LOCATION_SOURCE)

// Full source information should be included.
#define FROM_HERE FROM_HERE_WITH_EXPLICIT_FUNCTION(__func__)
#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name) \
    ::base::Location::createFromHere(function_name, __FILE__, __LINE__)

#else

#define FROM_HERE ::base::Location::createFromHere(__FILE__)
#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name) \
    ::base::Location::createFromHere(function_name, __FILE__, -1)

#endif

} // namespace base

#endif // BASE_LOCATION_H
