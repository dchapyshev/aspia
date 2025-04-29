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

#include "base/guid.h"

#include "base/endian_util.h"

#include <chrono>
#include <random>

namespace base {

namespace {

const size_t kGUIDLength = 36U;

//--------------------------------------------------------------------------------------------------
bool isHexDigit(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

//--------------------------------------------------------------------------------------------------
bool isLowerHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

//--------------------------------------------------------------------------------------------------
void numberToHex(char*& target, const char* source, size_t source_count)
{
    static const char kTable[] = "0123456789abcdef";

    for (size_t i = 0; i < source_count; ++i, target += 2)
    {
        target[0] = kTable[(source[i] >> 4) & 0x0F];
        target[1] = kTable[source[i] & 0x0F];
    }
}

//--------------------------------------------------------------------------------------------------
bool isValidGUIDInternal(std::string_view guid, bool strict)
{
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

//--------------------------------------------------------------------------------------------------
template <class T>
T randomDataToGUIDStringT(const uint64_t bytes[2])
{
    T result;
    result.resize(kGUIDLength);

    uint64_t big_endian[2];
    big_endian[0] = EndianUtil::toBig(bytes[0]);
    big_endian[1] = EndianUtil::toBig(bytes[1]);

    const char* source = reinterpret_cast<const char*>(&big_endian[0]);
    char* target = result.data();

    numberToHex(target, source, 4);
    *target++ = '-';
    numberToHex(target, source + 4, 2);
    *target++ = '-';
    numberToHex(target, source + 6, 2);
    *target++ = '-';
    numberToHex(target, source + 8, 2);
    *target++ = '-';
    numberToHex(target, source + 10, 6);

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Guid::Guid()
    : bytes_{0, 0}
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Guid::Guid(const Guid& other)
{
    bytes_[0] = other.bytes_[0];
    bytes_[1] = other.bytes_[1];
}

//--------------------------------------------------------------------------------------------------
Guid& Guid::operator=(const Guid& other)
{
    if (this != &other)
    {
        bytes_[0] = other.bytes_[0];
        bytes_[1] = other.bytes_[1];
    }

    return *this;
}

//--------------------------------------------------------------------------------------------------
Guid::Guid(const uint64_t bytes[2])
{
    bytes_[0] = bytes[0];
    bytes_[1] = bytes[1];
}

//--------------------------------------------------------------------------------------------------
bool Guid::isNull() const
{
    return !bytes_[0] || !bytes_[1];
}

//--------------------------------------------------------------------------------------------------
// static
Guid Guid::create()
{
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::mt19937_64 engine(seed);

    std::uniform_int_distribution<uint64_t> uniform_distance(
        std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max());

    uint64_t sixteen_bytes[2];
    sixteen_bytes[0] = uniform_distance(engine);
    sixteen_bytes[1] = uniform_distance(engine);

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

    return Guid(sixteen_bytes);
}

//--------------------------------------------------------------------------------------------------
// static
bool Guid::isValidGuidString(std::string_view guid)
{
    return isValidGUIDInternal(guid, false /* strict */);
}

//--------------------------------------------------------------------------------------------------
// static
bool Guid::isStrictValidGuidString(std::string_view guid)
{
    return isValidGUIDInternal(guid, true /* strict */);
}

//--------------------------------------------------------------------------------------------------
std::string Guid::toStdString() const
{
    return randomDataToGUIDStringT<std::string>(bytes_);
}

//--------------------------------------------------------------------------------------------------
bool Guid::operator==(const Guid& other) const
{
    return bytes_[0] == other.bytes_[0] && bytes_[1] == other.bytes_[1];
}

bool Guid::operator!=(const Guid& other) const
{
    return bytes_[0] != other.bytes_[0] || bytes_[1] != other.bytes_[1];
}

// static
std::string Guid::randomDataToGUIDString(const uint64_t bytes[2])
{
    return randomDataToGUIDStringT<std::string>(bytes);
}

} // namespace base
