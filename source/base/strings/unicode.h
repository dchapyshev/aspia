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

#ifndef BASE_STRINGS_UNICODE_H
#define BASE_STRINGS_UNICODE_H

#include "build/build_config.h"

#include <string>

namespace base {

//
// UTF-16 <-> UTF-8.
//

bool utf16ToUtf8(std::u16string_view in, std::string* out);
bool utf8ToUtf16(std::string_view in, std::u16string* out);

std::u16string utf16FromUtf8(std::string_view in);
std::string utf8FromUtf16(std::u16string_view in);

#if defined(OS_WIN)

//
// Wide <-> UTF-8.
//

bool wideToUtf8(std::wstring_view in, std::string* out);
bool utf8ToWide(std::string_view in, std::wstring* out);

std::wstring wideFromUtf8(std::string_view in);
std::string utf8FromWide(std::wstring_view in);

//
// Wide <-> UTF-16.
//

bool wideToUtf16(std::wstring_view in, std::u16string* out);
bool utf16ToWide(std::u16string_view in, std::wstring* out);

std::wstring wideFromUtf16(std::u16string_view in);
std::u16string utf16FromWide(std::wstring_view in);

#endif // #if defined(OS_WIN)

//
// ASCII <-> UTF-16.
//

std::string asciiFromUtf16(std::u16string_view in);
std::u16string utf16FromAscii(std::string_view in);

#if defined(OS_WIN)

//
// ASCII <-> Wide.
//

std::string asciiFromWide(std::wstring_view in);
std::wstring wideFromAscii(std::string_view in);

#endif // defined(OS_WIN)

//
// Local 8 bit <-> UTF-16.
//

bool utf16ToLocal8Bit(std::u16string_view in, std::string* out);
bool local8BitToUtf16(std::string_view in, std::u16string* out);

std::u16string utf16FromLocal8Bit(std::string_view in);
std::string local8BitFromUtf16(std::u16string_view in);

#if defined(OS_WIN)

//
// Local 8 bit <-> Wide.
//

bool wideToLocal8Bit(std::wstring_view in, std::string* out);
bool local8BitToWide(std::string_view in, std::wstring* out);

std::wstring wideFromLocal8Bit(std::string_view in);
std::string local8BitFromWide(std::wstring_view in);

#endif // defined(OS_WIN)

} // namespace base

#endif // BASE_STRINGS_UNICODE_H
