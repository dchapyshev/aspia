//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/strings/string_split.h"

#include "base/strings/string_util.h"
#include "base/strings/string_util_constants.h"

namespace base {

namespace {

// Returns either the ASCII or UTF-16 whitespace.
template <typename CharType>
std::basic_string_view<CharType> whitespaceForType();

//--------------------------------------------------------------------------------------------------
template <>
std::u16string_view whitespaceForType<char16_t>()
{
    return kWhitespaceUtf16;
}

//--------------------------------------------------------------------------------------------------
template <>
std::string_view whitespaceForType<char>()
{
    return kWhitespaceASCII;
}

//--------------------------------------------------------------------------------------------------
// Optimize the single-character case to call find() on the string instead, since this is the
// common case and can be made faster. This could have been done with template specialization too,
// but would have been less clear.
//
// There is no corresponding FindFirstNotOf because StringPiece already implements these different
// versions that do the optimized searching.
size_t findFirstOf(std::string_view piece, char c, size_t pos)
{
    return piece.find(c, pos);
}

//--------------------------------------------------------------------------------------------------
size_t findFirstOf(std::u16string_view piece, char16_t c, size_t pos)
{
    return piece.find(c, pos);
}

//--------------------------------------------------------------------------------------------------
size_t findFirstOf(std::string_view piece, std::string_view one_of, size_t pos)
{
    return piece.find_first_of(one_of, pos);
}

//--------------------------------------------------------------------------------------------------
size_t findFirstOf(std::u16string_view piece, std::u16string_view one_of, size_t pos)
{
    return piece.find_first_of(one_of, pos);
}

//--------------------------------------------------------------------------------------------------
// General string splitter template. Can take 8- or 16-bit input, can produce
// the corresponding string or StringPiece output, and can take single- or
// multiple-character delimiters.
//
// DelimiterType is either a character (Str::value_type) or a string piece of
// multiple characters (BasicStringPiece<Str>). StringPiece has a version of
// find for both of these cases, and the single-character version is the most
// common and can be implemented faster, which is why this is a template.
template<typename InputStringType, typename OutputStringType, typename DelimiterType>
std::vector<OutputStringType> splitStringT(InputStringType str,
                                           DelimiterType delimiter,
                                           WhitespaceHandling whitespace,
                                           SplitResult result_type)
{
    std::vector<OutputStringType> result;

    if (str.empty())
        return result;

    size_t start = 0;

    while (start != InputStringType::npos)
    {
        const size_t end = findFirstOf(str, delimiter, start);

        InputStringType piece;

        if (end == InputStringType::npos)
        {
            piece = str.substr(start);
            start = InputStringType::npos;
        }
        else
        {
            piece = str.substr(start, end - start);
            start = end + 1;
        }

        if (whitespace == TRIM_WHITESPACE)
            piece = trimString(piece, whitespaceForType<typename InputStringType::value_type>(), TRIM_ALL);

        if (result_type == SPLIT_WANT_ALL || !piece.empty())
            result.emplace_back(piece);
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
std::vector<std::string> splitString(std::string_view input,
                                     std::string_view separators,
                                     WhitespaceHandling whitespace,
                                     SplitResult result_type)
{
    if (separators.size() == 1)
    {
        return splitStringT<std::string_view, std::string, char>(
            input, separators[0], whitespace, result_type);
    }

    return splitStringT<std::string_view, std::string, std::string_view>(
        input, separators, whitespace, result_type);
}

//--------------------------------------------------------------------------------------------------
std::vector<std::u16string> splitString(std::u16string_view input,
                                        std::u16string_view separators,
                                        WhitespaceHandling whitespace,
                                        SplitResult result_type)
{
    if (separators.size() == 1)
    {
        return splitStringT<std::u16string_view, std::u16string, char16_t>(
            input, separators[0], whitespace, result_type);
    }

    return splitStringT<std::u16string_view, std::u16string, std::u16string_view>(
        input, separators, whitespace, result_type);
}

//--------------------------------------------------------------------------------------------------
std::vector<std::string_view> splitStringView(std::string_view input,
                                              std::string_view separators,
                                              WhitespaceHandling whitespace,
                                              SplitResult result_type)
{
    if (separators.size() == 1)
    {
        return splitStringT<std::string_view, std::string_view, char>(
            input, separators[0], whitespace, result_type);
    }

    return splitStringT<std::string_view, std::string_view, std::string_view>(
        input, separators, whitespace, result_type);
}

//--------------------------------------------------------------------------------------------------
std::vector<std::u16string_view> splitStringView(std::u16string_view input,
                                                 std::u16string_view separators,
                                                 WhitespaceHandling whitespace,
                                                 SplitResult result_type)
{
    if (separators.size() == 1)
    {
        return splitStringT<std::u16string_view, std::u16string_view, char16_t>(
            input, separators[0], whitespace, result_type);
    }

    return splitStringT<std::u16string_view, std::u16string_view, std::u16string_view>(
        input, separators, whitespace, result_type);
}

} // namespace base
