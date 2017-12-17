//
// PROJECT:         Aspia
// FILE:            base/strings/string_printf.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/strings/string_printf.h"
#include "base/logging.h"

#include <strsafe.h>

namespace aspia {

namespace {

HRESULT StringCchVPrintfT(char* buffer,
                          size_t buffer_size,
                          const char* format,
                          va_list args)
{
    return StringCchVPrintfA(buffer, buffer_size, format, args);
}

HRESULT StringCchVPrintfT(wchar_t* buffer,
                          size_t buffer_size,
                          const wchar_t* format,
                          va_list args)
{
    return StringCchVPrintfW(buffer, buffer_size, format, args);
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
StringType StringPrintfVT(const typename StringType::value_type* format,
                          va_list args)
{
    va_list args_copy;

    va_copy(args_copy, args);

    int length = vscprintfT(format, args_copy);
    CHECK(length >= 0) << errno;

    StringType result;
    result.resize(length);

    CHECK(SUCCEEDED(StringCchVPrintfT(&result[0], length + 1, format, args_copy)));

    va_end(args_copy);

    return result;
}

} // namespace

std::string StringPrintfV(const char* format, va_list args)
{
    return StringPrintfVT<std::string>(format, args);
}

std::wstring StringPrintfV(const wchar_t* format, va_list args)
{
    return StringPrintfVT<std::wstring>(format, args);
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
