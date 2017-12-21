//
// PROJECT:         Aspia
// FILE:            base/datetime.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/datetime.h"

#include <clocale>
#include <ctime>
#include <oleauto.h>

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

time_t SystemTimeToUnixTime(const SYSTEMTIME& system_time)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    tm.tm_year = system_time.wYear - 1900;
    tm.tm_mon = system_time.wMonth - 1;
    tm.tm_mday = system_time.wDay;

    tm.tm_hour = system_time.wHour;
    tm.tm_min = system_time.wMinute;
    tm.tm_sec = system_time.wSecond;
    tm.tm_isdst = -1;

    return mktime(&tm);
}

time_t VariantTimeToUnixTime(double variant_time)
{
    SYSTEMTIME system_time;
    memset(&system_time, 0, sizeof(system_time));

    VariantTimeToSystemTime(variant_time, &system_time);

    return SystemTimeToUnixTime(system_time);
}

} // namespace aspia
