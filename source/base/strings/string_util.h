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

#ifndef BASE_STRINGS_STRING_UTIL_H
#define BASE_STRINGS_STRING_UTIL_H

#include "build/build_config.h"

#include <string>

namespace base {

std::string replaceLfByCrLf(const std::string& in);
std::string replaceCrLfByLf(const std::string& in);

bool isStringUTF8(std::string_view str);
bool isStringASCII(std::string_view str);
bool isStringASCII(std::u16string_view str);

// Searches for CR or LF characters. Removes all contiguous whitespace strings that contain them.
// This is useful when trying to deal with text copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
std::u16string collapseWhitespace(std::u16string_view text,
                                  bool trim_sequences_with_line_breaks);

#if defined(OS_WIN)
std::wstring collapseWhitespace(std::wstring_view text,
                                bool trim_sequences_with_line_breaks);
#endif // defined(OS_WIN)

std::string collapseWhitespaceASCII(std::string_view text,
                                    bool trim_sequences_with_line_breaks);

int compareCaseInsensitiveASCII(std::string_view first, std::string_view second);
int compareCaseInsensitiveASCII(std::u16string_view first, std::u16string_view second);
int compareCaseInsensitive(std::u16string_view first, std::u16string_view second);

bool startsWith(std::string_view str, std::string_view search_for);
bool startsWith(std::u16string_view str, std::u16string_view search_for);
#if defined(OS_WIN)
bool startsWith(std::wstring_view str, std::wstring_view search_for);
#endif // defined(OS_WIN)

bool endsWith(std::string_view str, std::string_view search_for);
bool endsWith(std::u16string_view str, std::u16string_view search_for);
#if defined(OS_WIN)
bool endsWith(std::wstring_view str, std::wstring_view search_for);
#endif // defined(OS_WIN)

enum TrimPositions
{
    TRIM_NONE     = 0,
    TRIM_LEADING  = 1 << 0,
    TRIM_TRAILING = 1 << 1,
    TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

// Removes characters in |trim_chars| from the beginning and end of |input|.
// The 8-bit version only works on 8-bit characters, not UTF-8. Returns true if
// any characters were removed.
//
// It is safe to use the same variable for both |input| and |output| (this is
// the normal usage to trim in-place).
bool trimString(std::string_view input, std::string_view trim_chars, std::string* output);
bool trimString(std::u16string_view input, std::u16string_view trim_chars, std::u16string* output);

// string_view versions of the above. The returned pieces refer to the original buffer.
std::u16string_view trimString(std::u16string_view input,
                               std::u16string_view trim_chars,
                               TrimPositions positions);
std::string_view trimString(std::string_view input,
                            std::string_view trim_chars,
                            TrimPositions positions);

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
TrimPositions trimWhitespace(std::u16string_view input,
                             TrimPositions positions,
                             std::u16string* output);
std::u16string_view trimWhitespace(std::u16string_view input, TrimPositions positions);

TrimPositions trimWhitespaceASCII(std::string_view input,
                                  TrimPositions positions,
                                  std::string* output);
std::string_view trimWhitespaceASCII(std::string_view input, TrimPositions positions);

void removeChars(std::string* str, std::string_view substr);
void removeChars(std::wstring* str, std::wstring_view substr);

// ASCII-specific tolower.  The standard library's tolower is locale sensitive, so we don't want to
// use it here.

template <typename Char>
inline bool isUpperASCII(Char c)
{
    return c >= 'A' && c <= 'Z';
}
template <typename Char>
inline bool isLowerASCII(Char c)
{
    return c >= 'a' && c <= 'z';
}

inline char toLowerASCII(char c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

inline char16_t toLowerASCII(char16_t c)
{
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive, so we don't want to
// use it here.
inline char toUpperASCII(char c)
{
    return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

inline char16_t toUpperASCII(char16_t c)
{
    return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

std::u16string toUpperASCII(std::u16string_view in);
std::u16string toLowerASCII(std::u16string_view in);

std::string toUpperASCII(std::string_view in);
std::string toLowerASCII(std::string_view in);

std::u16string toUpper(std::u16string_view in);
std::u16string toLower(std::u16string_view in);

const std::string& emptyString();
const std::wstring& emptyStringW();

#if defined(WCHAR_T_IS_UTF16)

inline wchar_t* asWritableWide(char16_t* str)
{
    return reinterpret_cast<wchar_t*>(str);
}

inline wchar_t* asWritableWide(std::u16string& str)
{
    return reinterpret_cast<wchar_t*>(str.data());
}

inline const wchar_t* asWide(const char16_t* str)
{
    return reinterpret_cast<const wchar_t*>(str);
}

inline const wchar_t* asWide(const std::u16string& str)
{
    return reinterpret_cast<const wchar_t*>(str.data());
}

inline const wchar_t* asWide(std::u16string_view str)
{
    return reinterpret_cast<const wchar_t*>(str.data());
}

inline char16_t* asWritableUtf16(wchar_t* str)
{
    return reinterpret_cast<char16_t*>(str);
}

inline char16_t* asWritableUtf16(std::wstring& str)
{
    return reinterpret_cast<char16_t*>(str.data());
}

inline const char16_t* asUtf16(const wchar_t* str)
{
    return reinterpret_cast<const char16_t*>(str);
}

inline const char16_t* asUtf16(const std::wstring& str)
{
    return reinterpret_cast<const char16_t*>(str.data());
}

inline const char16_t* asUtf16(std::wstring_view str)
{
    return reinterpret_cast<const char16_t*>(str.data());
}

#endif // defined(WCHAR_T_IS_UTF16)

} // namespace base

#endif // BASE_STRINGS_STRING_UTIL_H
