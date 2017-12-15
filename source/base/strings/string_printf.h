//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/string_printf.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__STRINGS__STRING_PRINTF_H
#define _ASPIA_BASE__STRINGS__STRING_PRINTF_H

#include <string>

namespace aspia {

// Return a C++ string given vprintf-like input.
std::string StringPrintfV(const char* format, va_list args);
std::wstring StringPrintfV(const wchar_t* format, va_list args);

// Return a C++ string given printf-like input.
std::string StringPrintf(const char* format, ...);
std::wstring StringPrintf(const wchar_t* format, ...);

// Store result into a supplied string and return it.
const std::string& SStringPrintf(std::string& dst, const char* format, ...);
const std::wstring& SStringPrintf(std::wstring& dst, const wchar_t* format, ...);

} // namespace aspia

#endif // _ASPIA_BASE__STRINGS__STRING_PRINTF_H
