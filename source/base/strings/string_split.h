//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__STRINGS__STRING_SPLIT_H
#define BASE__STRINGS__STRING_SPLIT_H

#include <string>
#include <vector>

namespace base {

enum WhitespaceHandling
{
    KEEP_WHITESPACE,
    TRIM_WHITESPACE
};

enum SplitResult
{
    // Strictly return all results.
    //
    // If the input is ",," and the separator is ',' this will return a vector of three empty
    // strings.
    SPLIT_WANT_ALL,

    // Only nonempty results will be added to the results. Multiple separators will be coalesced.
    // Separators at the beginning and end of the input will be ignored. With TRIM_WHITESPACE,
    // whitespace-only results will be dropped.
    //
    // If the input is ",," and the separator is ',', this will return an empty vector.
    SPLIT_WANT_NONEMPTY
};

// Split the given string on ANY of the given separators, returning copies of
// the result.
//
// To split on either commas or semicolons, keeping all whitespace:
//
//   std::vector<std::string> tokens = base::splitString(
//       input, ",;", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
std::vector<std::string> splitString(std::string_view input,
                                     std::string_view separators,
                                     WhitespaceHandling whitespace,
                                     SplitResult result_type);

std::vector<std::u16string> splitString(std::u16string_view input,
                                        std::u16string_view separators,
                                        WhitespaceHandling whitespace,
                                        SplitResult result_type);

// Like splitString above except it returns a vector of std::*string_view which reference the
// original buffer without copying. Although you have to be careful to keep the original string
// unmodified, this provides an efficient way to iterate through tokens in a string.
//
// To iterate through all whitespace-separated tokens in an input string:
//
//   for (const auto& cur :
//        base::splitStringView(input, base::kWhitespaceASCII,
//                              base::KEEP_WHITESPACE,
//                              base::SPLIT_WANT_NONEMPTY)) {
//     ...
std::vector<std::string_view> splitStringView(std::string_view input,
                                              std::string_view separators,
                                              WhitespaceHandling whitespace,
                                              SplitResult result_type);

std::vector<std::u16string_view> splitStringView(std::u16string_view input,
                                                 std::u16string_view separators,
                                                 WhitespaceHandling whitespace,
                                                 SplitResult result_type);

} // namespace base

#endif // BASE__STRINGS__STRING_SPLIT_H
