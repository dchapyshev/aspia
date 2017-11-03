//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/logging.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOGGING_H
#define _ASPIA_BASE__LOGGING_H

#define GLOG_NO_ABBREVIATED_SEVERITIES
#define GOOGLE_GLOG_DLL_DECL

#pragma warning(push, 3)
#include <glog/logging.h>
#pragma warning(pop)

namespace aspia {

using SystemErrorCode = DWORD;

SystemErrorCode GetLastSystemErrorCode();
std::string SystemErrorCodeToString(SystemErrorCode error_code);
std::string GetLastSystemErrorString();

std::ostream& operator<<(std::ostream& out, const wchar_t* str);
std::ostream& operator<<(std::ostream& out, const std::wstring& str);

} // namespace aspia

#endif // _ASPIA_BASE__LOGGING_H
