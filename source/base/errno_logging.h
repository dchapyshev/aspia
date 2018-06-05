//
// PROJECT:         Aspia
// FILE:            base/errno_logging.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__ERRNO_LOGGING_H
#define _ASPIA_BASE__ERRNO_LOGGING_H

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

#endif // _ASPIA_BASE__ERRNO_LOGGING_H
