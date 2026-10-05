//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/credentials/logging.h"

#include <windows.h>
#include <strsafe.h>

#include <cstdarg>

namespace {

//--------------------------------------------------------------------------------------------------
const char* baseName(const char* path)
{
    const char* name = path;
    for (const char* p = path; *p; ++p)
    {
        if (*p == '\\' || *p == '/')
            name = p + 1;
    }
    return name;
}

//--------------------------------------------------------------------------------------------------
void writeToFile(const wchar_t* text)
{
    wchar_t path[MAX_PATH] = {};
    DWORD length = GetTempPathW(ARRAYSIZE(path), path);
    if (length == 0 || length > ARRAYSIZE(path))
        return;

    if (FAILED(StringCchCatW(path, ARRAYSIZE(path), L"aspia_credentials.log")))
        return;

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    char utf8[2560] = {};
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (utf8_size > 1)
    {
        DWORD written = 0;
        WriteFile(file, utf8, static_cast<DWORD>(utf8_size - 1), &written, nullptr);
    }

    CloseHandle(file);
}

} // namespace

//--------------------------------------------------------------------------------------------------
void writeLog(const char* file, int line, const char* function, const wchar_t* format, ...)
{
    wchar_t message[1024] = {};

    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);

    SYSTEMTIME time = {};
    GetLocalTime(&time);

    wchar_t line_buffer[1280] = {};
    StringCchPrintfW(line_buffer, ARRAYSIZE(line_buffer),
        L"%02u:%02u:%02u.%03u %hs(%d) %hs: %s\r\n",
        time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
        baseName(file), line, function, message);

    writeToFile(line_buffer);
}
