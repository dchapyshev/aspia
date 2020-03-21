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

#include "base/location.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_printf.h"

#if defined(CC_MSVC)
#include <intrin.h>
#endif

namespace base {

Location::Location() = default;
Location::Location(const Location& other) = default;
Location& Location::operator=(const Location& other) = default;

Location::Location(const char* file_name, const void* program_counter)
    : file_name_(file_name),
      program_counter_(program_counter)
{
    // Nothing
}

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number,
                   const void* program_counter)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number),
      program_counter_(program_counter)
{
#if !defined(OS_NACL)
    // The program counter should not be null except in a default constructed
    // (empty) Location object. This value is used for identity, so if it doesn't
    // uniquely identify a location, things will break.
    //
    // The program counter isn't supported in NaCl so location objects won't work
    // properly in that context.
    DCHECK(program_counter);
#endif
}

std::string Location::toString(PathType path_type) const
{
    if (hasSourceInfo())
    {
        std::string_view file_name(file_name_);

        if (path_type == SHORT_PATH)
        {
            size_t last_slash_pos = file_name.find_last_of("\\/");
            if (last_slash_pos != std::string_view::npos)
                file_name.remove_prefix(last_slash_pos + 1);
        }

        return std::string(function_name_) + "@" +
               std::string(file_name) + ":" +
               numberToString(line_number_);
    }

    return stringPrintf("pc:%p", program_counter_);
}

#if defined(CC_MSVC)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(CC_GCC) && !defined(OS_NACL)
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

// static
NOINLINE Location Location::createFromHere(const char* file_name)
{
    return Location(file_name, RETURN_ADDRESS());
}

// static
NOINLINE Location Location::createFromHere(const char* function_name,
                                           const char* file_name,
                                           int line_number)
{
    return Location(function_name, file_name, line_number, RETURN_ADDRESS());
}

} // namespace base
