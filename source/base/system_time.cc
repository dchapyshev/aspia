//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/system_time.h"

#include "build/build_config.h"

#include <cstring>

#if defined(OS_WIN)
#include <Windows.h>
#elif defined(OS_POSIX)
#include <sys/time.h>
#include <ctime>
#endif // defined(OS_WIN)

namespace base {

//--------------------------------------------------------------------------------------------------
SystemTime::SystemTime(const Data& data)
    : data_(data)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
// static
SystemTime SystemTime::now()
{
    Data data;
    memset(&data, 0, sizeof(data));

#if defined(OS_WIN)
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    data.year        = local_time.wYear;
    data.month       = local_time.wMonth;
    data.day         = local_time.wDay;
    data.hour        = local_time.wHour;
    data.minute      = local_time.wMinute;
    data.second      = local_time.wSecond;
    data.millisecond = local_time.wMilliseconds;
#elif defined(OS_POSIX)
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;

    data.year        = 1900 + tm_time->tm_year;
    data.month       = 1 + tm_time->tm_mon;
    data.day         = tm_time->tm_mday;
    data.hour        = tm_time->tm_hour;
    data.minute      = tm_time->tm_min;
    data.second      = tm_time->tm_sec;
    data.millisecond = tv.tv_usec;
#else
#error Platform support not implemented
#endif

    return SystemTime(data);
}

} // namespace base
