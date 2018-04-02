//
// PROJECT:         Aspia
// FILE:            base/system_error_code.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/system_error_code.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(Q_OS_WIN)

namespace aspia {

SystemErrorCode lastSystemErrorCode()
{
#if defined(Q_OS_WIN)
    return GetLastError();
#else
#error Platform support not implemented
#endif
}

void setLastSystemErrorCode(SystemErrorCode error_code)
{
#if defined(Q_OS_WIN)
    SetLastError(error_code);
#else
#error Platform support not implemented
#endif
}

QString systemErrorCodeToString(SystemErrorCode error_code)
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
        return QString::fromUtf16(reinterpret_cast<const ushort*>(buffer)).trimmed();

    return QString("Error (%1) while retrieving error (%2")
        .arg(lastSystemErrorCode())
        .arg(error_code);
#else
#error Platform support not implemented
#endif
}

QString lastSystemErrorString()
{
    SystemErrorCode error_code = lastSystemErrorCode();

    QString error_string = systemErrorCodeToString(error_code);

    setLastSystemErrorCode(error_code);
    return error_string;
}

} // namespace aspia
