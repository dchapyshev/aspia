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

#ifndef BASE__LOCATION_H
#define BASE__LOCATION_H

#include "build/build_config.h"

#include <string>

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
    Location(const char* file_name, const void* program_counter);

    // Constructor should be called with a long-lived char*, such as __FILE__. It assumes the
    // provided value will persist as a global constant, and it will not make a copy of it.
    Location(const char* function_name,
             const char* file_name,
             int line_number,
             const void* program_counter);

    // Comparator for hash map insertion. The program counter should uniquely identify a location.
    bool operator==(const Location& other) const
    {
        return program_counter_ == other.program_counter_;
    }

    // Returns true if there is source code location info. If this is false, the Location object
    // only contains a program counter or is default-initialized (the program counter is also null).
    bool hasSourceInfo() const { return function_name_ && file_name_; }

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* functionName() const { return function_name_; }

    // Will be nullptr for default initialized Location objects and when source names are disabled.
    const char* fileName() const { return file_name_; }

    // Will be -1 for default initialized Location objects and when source names are disabled.
    int lineNumber() const { return line_number_; }

    // The address of the code generating this Location object. Should always be valid except for
    // default initialized Location objects, which will be nullptr.
    const void* programCounter() const { return program_counter_; }

    enum PathType
    {
        FULL_PATH,
        SHORT_PATH
    };

    // Converts to the most user-readable form possible. If function and filename
    // are not available, this will return "pc:<hex address>".
    std::string toString(PathType path_type = SHORT_PATH) const;

    static Location createFromHere(const char* file_name);
    static Location createFromHere(const char* function_name,
                                   const char* file_name,
                                   int line_number);

private:
    const char* function_name_ = nullptr;
    const char* file_name_ = nullptr;
    int line_number_ = -1;
    const void* program_counter_ = nullptr;
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

#endif // BASE__LOCATION_H
