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

#ifndef ASPIA_BASE__ERRNO_LOGGING_H_
#define ASPIA_BASE__ERRNO_LOGGING_H_

#include <QString>

namespace aspia {

#if defined(Q_OS_WIN)
using SystemErrorCode = unsigned long;
#elif defined(Q_OS_UNIX)
using SystemErrorCode = int;
#endif

QString errnoToString(SystemErrorCode error_code);

void errnoToLog(QtMsgType type, const char* file, int line, const char* message, ...);

#define qDebugErrno(_msg_, ...) \
    errnoToLog(QtDebugMsg, __FILE__, __LINE__, _msg_, ##__VA_ARGS__)

#define qWarningErrno(_msg_, ...) \
    errnoToLog(QtWarningMsg, __FILE__, __LINE__, _msg_, ##__VA_ARGS__)

#define qCriticalErrno(_msg_, ...) \
    errnoToLog(QtCriticalMsg, __FILE__, __LINE__, _msg_, ##__VA_ARGS__)

#define qFatalErrno(_msg_, ...) \
    errnoToLog(QtFatalMsg, __FILE__, __LINE__, _msg_, ##__VA_ARGS__)

#define qInfoErrno(_msg_, ...) \
    errnoToLog(QtInfoMsg, __FILE__, __LINE__, _msg_, ##__VA_ARGS__)

} // namespace aspia

#endif // ASPIA_BASE__ERRNO_LOGGING_H_
