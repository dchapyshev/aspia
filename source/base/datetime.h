//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/datetime.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__DATETIME_H
#define _ASPIA_BASE__DATETIME_H

#include <string>

namespace aspia {

std::wstring TimeToStringW(time_t time);

std::string TimeToString(time_t time);

} // namespace aspia

#endif // _ASPIA_BASE__DATETIME_H
