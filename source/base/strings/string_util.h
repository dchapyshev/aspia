//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/string_util.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__STRINGS__STRING_UTIL_H
#define _ASPIA_BASE__STRINGS__STRING_UTIL_H

#include <string>

namespace aspia {

std::string ReplaceLfByCrLf(const std::string& in);

std::string ReplaceCrLfByLf(const std::string& in);

bool StringIsUtf8(const char* data, size_t length);

std::string StringPrintf(const char* format, ...);

std::wstring StringPrintfW(const WCHAR* format, ...);

// Searches  for CR or LF characters. Removes all contiguous whitespace
// strings that contain them. This is useful when trying to deal with text
// copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
std::wstring CollapseWhitespace(const std::wstring& text,
                                bool trim_sequences_with_line_breaks);
std::string CollapseWhitespaceASCII(const std::string& text,
                                    bool trim_sequences_with_line_breaks);

int CompareCaseInsensitive(const std::string& first, const std::string& second);
int CompareCaseInsensitive(const std::wstring& first, const std::wstring& second);

enum TrimPositions
{
    TRIM_NONE     = 0,
    TRIM_LEADING  = 1 << 0,
    TRIM_TRAILING = 1 << 1,
    TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
TrimPositions TrimWhitespace(const std::wstring& input,
                             TrimPositions positions,
                             std::wstring& output);
TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string& output);

void ToUpper(std::wstring& string);
std::wstring ToUpperCopy(const std::wstring& string);

void ToLower(std::wstring& string);
std::wstring ToLowerCopy(const std::wstring& string);

} // namespace aspia

#endif // _ASPIA_BASE__STRINGS__STRING_UTIL_H
