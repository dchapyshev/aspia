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

#ifndef BASE__STRINGS__STRING_NUMBER_CONVERSIONS_H
#define BASE__STRINGS__STRING_NUMBER_CONVERSIONS_H

#include <cstdint>
#include <string>

// DO NOT use these functions in any UI unless it's NOT localized on purpose.
// Using these functions in the UI would lead numbers to be formatted in a non-native way.

namespace base {

// Functions to convert a string to a number.
// The string must contain only a number, no other characters are allowed.
// Positive numbers should not have the '+' symbol.
// The value |output| changes only when the conversion is successful. |output| cannot be equal to
// nullptr.
// If it was possible to convert the string to a number, then true is returned, otherwise false.

bool stringToInt(std::string_view input, int* output);
bool stringToUint(std::string_view input, unsigned* output);
bool stringToULong(std::string_view input, unsigned long* output);
bool stringToInt64(std::string_view input, int64_t* output);
bool stringToUint64(std::string_view input, uint64_t* output);
bool stringToSizeT(std::string_view input, size_t* output);

// Functions to convert a number to a string.

std::string numberToString(int value);
std::string numberToString(unsigned int value);
std::string numberToString(long value);
std::string numberToString(unsigned long value);
std::string numberToString(long long value);
std::string numberToString(unsigned long long value);

} // namespace base

#endif // BASE__STRINGS__STRING_NUMBER_CONVERSIONS_H
