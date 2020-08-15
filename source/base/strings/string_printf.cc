//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/strings/string_printf.h"

#include <cstdarg>
#include <cstddef>

namespace base {

namespace {

int vsnprintfT(char* buffer, std::size_t buffer_size, const char* format, va_list args)
{
    return _vsnprintf_s(buffer, buffer_size, _TRUNCATE, format, args);
}

int vsnprintfT(wchar_t* buffer, std::size_t buffer_size, const wchar_t* format, va_list args)
{
    return _vsnwprintf_s(buffer, buffer_size, _TRUNCATE, format, args);
}

int vscprintfT(const char* format, va_list args)
{
    return _vscprintf(format, args);
}

int vscprintfT(const wchar_t* format, va_list args)
{
    return _vscwprintf(format, args);
}

template<class StringType>
StringType stringPrintfVT(const typename StringType::value_type* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    const int length = vscprintfT(format, args_copy);
    if (length <= 0)
    {
        va_end(args_copy);
        return StringType();
    }

    StringType result;
    result.resize(length);

    const int ret = vsnprintfT(result.data(), length + 1, format, args_copy);
    va_end(args_copy);

    if (ret < 0 || ret > length)
        return StringType();

    return result;
}

} // namespace

std::string stringPrintfV(const char* format, va_list args)
{
    return stringPrintfVT<std::string>(format, args);
}

std::wstring stringPrintfV(const wchar_t* format, va_list args)
{
    return stringPrintfVT<std::wstring>(format, args);
}

std::string stringPrintf(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    std::string result = stringPrintfV(format, args);
    va_end(args);

    return result;
}

std::wstring stringPrintf(const wchar_t* format, ...)
{
    va_list args;

    va_start(args, format);
    std::wstring result = stringPrintfV(format, args);
    va_end(args);

    return result;
}

const std::string& sStringPrintf(std::string* dst, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    *dst = stringPrintfV(format, args);
    va_end(args);

    return *dst;
}

const std::wstring& sStringPrintf(std::wstring* dst, const wchar_t* format, ...)
{
    va_list args;

    va_start(args, format);
    *dst = stringPrintfV(format, args);
    va_end(args);

    return *dst;
}

} // namespace base
