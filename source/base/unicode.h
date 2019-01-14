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

#ifndef ASPIA_BASE__UNICODE_H
#define ASPIA_BASE__UNICODE_H

#include <string>

namespace base {

bool UTF16toUTF8(const std::wstring& in, std::string* out);
bool UTF8toUTF16(const std::string& in, std::wstring* out);

bool UTF16toUTF8(const wchar_t* in, std::string* out);
bool UTF8toUTF16(const char* in, std::wstring* out);

std::wstring UTF16fromUTF8(const std::string& in);
std::string UTF8fromUTF16(const std::wstring& in);

std::wstring UTF16fromUTF8(const char* in);
std::string UTF8fromUTF16(const wchar_t* in);

} // namespace base

#endif // ASPIA_BASE__UNICODE_H
