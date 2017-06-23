//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/string_util.h
// LICENSE:         See top-level directory
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

} // namespace aspia

#endif // _ASPIA_BASE__STRINGS__STRING_UTIL_H
