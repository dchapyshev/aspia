//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/strings/string_printf.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/logging.h"

#include <strsafe.h>

namespace aspia {

std::string StringPrintfV(const char* format, va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);

    int length = _vscprintf(format, args_copy);
    CHECK(length >= 0) << errno;

    std::string result;
    result.resize(length);

    CHECK(SUCCEEDED(StringCchVPrintfA(&result[0], length + 1, format, args_copy)));

    va_end(args_copy);

    return result;
}

std::wstring StringPrintfV(const wchar_t* format, va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);

    int length = _vscwprintf(format, args_copy);
    CHECK(length >= 0) << errno;

    std::wstring result;
    result.resize(length);

    CHECK(SUCCEEDED(StringCchVPrintfW(&result[0], length + 1, format, args_copy)));

    va_end(args_copy);

    return result;
}

std::string StringPrintf(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    std::string result = StringPrintfV(format, args);
    va_end(args);

    return result;
}

std::wstring StringPrintf(const wchar_t* format, ...)
{
    va_list args;

    va_start(args, format);
    std::wstring result = StringPrintfV(format, args);
    va_end(args);

    return result;
}

const std::string& SStringPrintf(std::string& dst, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    dst = StringPrintfV(format, args);
    va_end(args);

    return dst;
}

const std::wstring& SStringPrintf(std::wstring& dst, const wchar_t* format, ...)
{
    va_list args;

    va_start(args, format);
    dst = StringPrintfV(format, args);
    va_end(args);

    return dst;
}

} // namespace aspia
