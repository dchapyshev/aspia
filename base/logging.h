//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/logging.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOGGING_H
#define _ASPIA_BASE__LOGGING_H

#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL
#include <glog/logging.h>

namespace aspia {

using SystemErrorCode = DWORD;

SystemErrorCode GetLastSystemErrorCode();
std::string SystemErrorCodeToString(SystemErrorCode error_code);

} // namespace aspia

#endif // _ASPIA_BASE__LOGGING_H
