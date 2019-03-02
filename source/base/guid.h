//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__GUID_H
#define BASE__GUID_H

#include "base/macros_magic.h"

#include <string>

namespace base {

class Guid
{
public:
    // Generate a 128-bit random GUID in the form of version 4 as described in
    // RFC 4122, section 4.4.
    // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
    // where y is one of [8, 9, A, B].
    // The hexadecimal values "a" through "f" are output as lower case characters.
    //
    // A cryptographically secure random source will be used, but consider using
    // UnguessableToken for greater type-safety if GUID format is unnecessary.
    static std::string create();

    // Returns true if the input string conforms to the version 4 GUID format.
    // Note that this does NOT check if the hexadecimal values "a" through "f"
    // are in lower case characters, as Version 4 RFC says onput they're
    // case insensitive. (Use isStrictValid for checking if the
    // given string is valid output string)
    static bool isValid(const std::string& guid);

    // Returns true if the input string is valid version 4 GUID output string.
    // This also checks if the hexadecimal values "a" through "f" are in lower
    // case characters.
    static bool isStrictValid(const std::string& guid);

    // For unit testing purposes only. Do not use outside of tests.
    static std::string randomDataToGUIDString(const uint64_t bytes[2]);

private:
    DISALLOW_COPY_AND_ASSIGN(Guid);
};

} // namespace base

#endif // BASE__GUID_H
