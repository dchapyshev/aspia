//
// PROJECT:         Aspia
// FILE:            base/system_error_code.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SYSTEM_ERROR_CODE_H
#define _ASPIA_BASE__SYSTEM_ERROR_CODE_H

#include <QString>

namespace aspia {

#if defined(Q_OS_WIN)
using SystemErrorCode = unsigned long;
#elif defined(Q_OS_UNIX)
using SystemErrorCode = int;
#endif

SystemErrorCode lastSystemErrorCode();
void setLastSystemErrorCode(SystemErrorCode error_code);
QString systemErrorCodeToString(SystemErrorCode error_code);
QString lastSystemErrorString();

} // namespace aspia

#endif // _ASPIA_BASE__SYSTEM_ERROR_CODE_H
