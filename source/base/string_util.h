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

#ifndef ASPIA_BASE__STRING_UTIL_H_
#define ASPIA_BASE__STRING_UTIL_H_

#include <string>

namespace aspia {

std::string replaceLfByCrLf(const std::string& in);
std::string replaceCrLfByLf(const std::string& in);

bool isStringUTF8(const char* data, size_t length);
bool isStringUTF8(const std::string& string);

bool isStringASCII(const char* data, size_t length);
bool isStringASCII(const std::string& string);

bool isStringASCII(const wchar_t* data, size_t length);
bool isStringASCII(const std::wstring& string);

// Searches  for CR or LF characters. Removes all contiguous whitespace
// strings that contain them. This is useful when trying to deal with text
// copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
std::wstring collapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks);
std::string collapseWhitespaceASCII(const std::string& text,
                                    bool trim_sequences_with_line_breaks);

int compareCaseInsensitiveASCII(const std::string& first, const std::string& second);
int compareCaseInsensitive(const std::wstring& first, const std::wstring& second);

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
bool trimString(const std::string& input, std::string_view trim_chars, std::string& output);
bool trimString(const std::wstring& input, std::wstring_view trim_chars, std::wstring& output);

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
TrimPositions trimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring& output);
TrimPositions trimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string& output);

std::wstring toUpper(std::wstring_view in);
std::wstring toLower(std::wstring_view in);

std::string toUpperASCII(std::string_view in);
std::string toLowerASCII(std::string_view in);

const std::string& emptyString();
const std::wstring& emptyStringW();

} // namespace aspia

#endif // ASPIA_BASE__STRINGS__STRING_UTIL_H_
