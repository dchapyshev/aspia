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

#ifndef ASPIA_BASE__STRING_PRINTF_H
#define ASPIA_BASE__STRING_PRINTF_H

#include <string>

namespace base {

// Return a C++ string given vprintf-like input.
std::string stringPrintfV(const char* format, va_list args);
std::wstring stringPrintfV(const wchar_t* format, va_list args);

// Return a C++ string given printf-like input.
std::string stringPrintf(const char* format, ...);
std::wstring stringPrintf(const wchar_t* format, ...);

// Store result into a supplied string and return it.
const std::string& sStringPrintf(std::string* dst, const char* format, ...);
const std::wstring& sStringPrintf(std::wstring* dst, const wchar_t* format, ...);

} // namespace base

#endif // ASPIA_BASE__STRING_PRINTF_H
