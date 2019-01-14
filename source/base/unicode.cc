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

#include "base/unicode.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace base {

namespace {

// TODO: Cross-platform implementation.
bool Utf16ToUtf8Impl(std::wstring_view in, std::string* out)
{
    size_t in_len = in.length();
    if (!in_len)
        return false;

    int out_len = WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                                      nullptr, 0, nullptr, nullptr);
    if (out_len <= 0)
        return false;

    out->resize(out_len);

    if (WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                            out->data(), out_len, nullptr, nullptr) != out_len)
        return false;

    return true;
}

// TODO: Cross-platform implementation.
bool Utf8ToUtf16Impl(std::string_view in, std::wstring* out)
{
    size_t in_len = in.length();
    if (!in_len)
        return false;

    int out_len = MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in_len), nullptr, 0);
    if (out_len <= 0)
        return false;

    out->resize(out_len);

    if (MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in_len),
                            out->data(), out_len) != out_len)
        return false;

    return true;
}

} // namespace

bool UTF16toUTF8(const std::wstring& in, std::string* out)
{
    return Utf16ToUtf8Impl(in, out);
}

bool UTF8toUTF16(const std::string& in, std::wstring* out)
{
    return Utf8ToUtf16Impl(in, out);
}

bool UTF16toUTF8(const wchar_t* in, std::string* out)
{
    if (!in)
        return false;

    return Utf16ToUtf8Impl(in, out);
}

bool UTF8toUTF16(const char* in, std::wstring* out)
{
    if (!in)
        return false;

    return Utf8ToUtf16Impl(in, out);
}

std::wstring UTF16fromUTF8(const std::string& in)
{
    std::wstring out;
    UTF8toUTF16(in, &out);
    return out;
}

std::string UTF8fromUTF16(const std::wstring& in)
{
    std::string out;
    UTF16toUTF8(in, &out);
    return out;
}

std::wstring UTF16fromUTF8(const char* in)
{
    std::wstring out;
    UTF8toUTF16(in, &out);
    return out;
}

std::string UTF8fromUTF16(const wchar_t* in)
{
    std::string out;
    UTF16toUTF8(in, &out);
    return out;
}

} // namespace base
