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
#include "crypto/random.h"

namespace aspia {

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

std::string randomDataToGUIDString(const uint64_t bytes[2])
{
    return stringPrintf("%08x-%04x-%04x-%04x-%012llx",
                        static_cast<unsigned int>(bytes[0] >> 32),
                        static_cast<unsigned int>((bytes[0] >> 16) & 0x0000ffff),
                        static_cast<unsigned int>(bytes[0] & 0x0000ffff),
                        static_cast<unsigned int>(bytes[1] >> 48),
                        bytes[1] & 0x0000ffff'ffffffffULL);
}

} // namespace

// static
std::string Guid::create()
{
    uint64_t sixteen_bytes[2];

    // Use base::RandBytes instead of crypto::RandBytes, because crypto calls the
    // base version directly, and to prevent the dependency from base/ to crypto/.
    Random::fillBuffer(&sixteen_bytes, sizeof(sixteen_bytes));

    // Set the GUID to version 4 as described in RFC 4122, section 4.4.
    // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
    // where y is one of [8, 9, A, B].

    // Clear the version bits and set the version to 4:
    sixteen_bytes[0] &= 0xffffffff'ffff0fffULL;
    sixteen_bytes[0] |= 0x00000000'00004000ULL;

    // Set the two most significant bits (bits 6 and 7) of the
    // clock_seq_hi_and_reserved to zero and one, respectively:
    sixteen_bytes[1] &= 0x3fffffff'ffffffffULL;
    sixteen_bytes[1] |= 0x80000000'00000000ULL;

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

} // namespace aspia
