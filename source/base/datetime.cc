//
// PROJECT:         Aspia
// FILE:            base/datetime.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/datetime.h"

#include <clocale>
#include <ctime>

namespace aspia {

std::wstring TimeToStringW(time_t time)
{
    tm* local_time = std::localtime(&time);

    if (!local_time)
        return std::wstring();

    // Set the locale obtained from system.
    std::setlocale(LC_TIME, "");

    wchar_t string[128];
    if (!std::wcsftime(string, _countof(string), L"%x %X", local_time))
        return std::wstring();

    return string;
}

std::string TimeToString(time_t time)
{
    tm* local_time = std::localtime(&time);

    if (!local_time)
        return std::string();

    // Set the locale obtained from system.
    std::setlocale(LC_TIME, "");

    char string[128];
    if (!std::strftime(string, _countof(string), "%x %X", local_time))
        return std::string();

    return string;
}

} // namespace aspia
