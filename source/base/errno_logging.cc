//
// PROJECT:         Aspia
// FILE:            base/errno_logging.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/errno_logging.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(Q_OS_WIN)

namespace aspia {

QString errnoToString(SystemErrorCode error_code)
{
#if defined(Q_OS_WIN)
    const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    const int kErrorMessageBufferSize = 256;
    wchar_t buffer[kErrorMessageBufferSize];

    DWORD length = FormatMessageW(flags,
                                  nullptr,
                                  error_code,
                                  0,
                                  buffer,
                                  kErrorMessageBufferSize,
                                  nullptr);
    if (length)
    {
        return QString("%1 (%2)")
            .arg(QString::fromUtf16(reinterpret_cast<const ushort*>(buffer)).trimmed())
            .arg(error_code);
    }

    return QString("Error (%1) while retrieving error (%2)")
        .arg(GetLastError())
        .arg(error_code);
#else
#error Platform support not implemented
#endif
}

void errnoToLog(QtMsgType type, const char* file, int line, const char* message, ...)
{
    SystemErrorCode error_code = GetLastError();

    QString buffer;

    va_list ap;
    va_start(ap, message);
    if (message)
        buffer.vsprintf(message, ap);
    va_end(ap);

    buffer += QLatin1String(": ") + errnoToString(error_code);

    QMessageLogContext context;
    context.file = file;
    context.line = line;

    qt_message_output(type, context, buffer);

    SetLastError(error_code);
}

} // namespace aspia
