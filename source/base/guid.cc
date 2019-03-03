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

#include "base/guid.h"
#include "base/string_printf.h"

#include <random>

namespace base {

namespace {

bool isHexDigit(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

bool isLowerHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool isValidGUIDInternal(const std::string& guid, bool strict)
{
    const size_t kGUIDLength = 36U;
    if (guid.length() != kGUIDLength)
        return false;

    for (size_t i = 0; i < guid.length(); ++i)
    {
        char current = guid[i];
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (current != '-')
                return false;
        }
        else
        {
            if ((strict && !isLowerHexDigit(current)) || !isHexDigit(current))
                return false;
        }
    }

    return true;
}

} // namespace

// static
std::string Guid::create()
{
    std::random_device random_device;
    std::mt19937 mt(random_device());
    std::uniform_int_distribution<uint64_t> uniform_distance(
        std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());

    uint64_t sixteen_bytes[2];
    sixteen_bytes[0] = uniform_distance(mt);
    sixteen_bytes[1] = uniform_distance(mt);

    // Set the GUID to version 4 as described in RFC 4122, section 4.4.
    // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
    // where y is one of [8, 9, A, B].

    // Clear the version bits and set the version to 4:
    sixteen_bytes[0] &= 0xffffffffffff0fffULL;
    sixteen_bytes[0] |= 0x0000000000004000ULL;

    // Set the two most significant bits (bits 6 and 7) of the
    // clock_seq_hi_and_reserved to zero and one, respectively:
    sixteen_bytes[1] &= 0x3fffffffffffffffULL;
    sixteen_bytes[1] |= 0x8000000000000000ULL;

    return randomDataToGUIDString(sixteen_bytes);
}

// static
bool Guid::isValid(const std::string& guid)
{
    return isValidGUIDInternal(guid, false /* strict */);
}

// static
bool Guid::isStrictValid(const std::string& guid)
{
    return isValidGUIDInternal(guid, true /* strict */);
}

// static
std::string Guid::randomDataToGUIDString(const uint64_t bytes[2])
{
    return stringPrintf("%08x-%04x-%04x-%04x-%012llx",
                        static_cast<unsigned int>(bytes[0] >> 32),
                        static_cast<unsigned int>((bytes[0] >> 16) & 0x0000ffff),
                        static_cast<unsigned int>(bytes[0] & 0x0000ffff),
                        static_cast<unsigned int>(bytes[1] >> 48),
                        bytes[1] & 0x0000ffffffffffffULL);
}

} // namespace base
