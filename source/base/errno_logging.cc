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
