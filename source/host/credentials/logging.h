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

#ifndef HOST_CREDENTIALS_LOGGING_H
#define HOST_CREDENTIALS_LOGGING_H

void writeLog(const char* file, int line, const char* function, const wchar_t* format, ...);

#if defined(NDEBUG)
#define LOG(format, ...) \
    ((void)sizeof(writeLog(__FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__), 0))
#else
#define LOG(format, ...) \
    writeLog(__FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#endif

#endif // HOST_CREDENTIALS_LOGGING_H
