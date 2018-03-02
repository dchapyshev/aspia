//
// PROJECT:         Aspia
// FILE:            ui/misc.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__UI_UTIL_H
#define _ASPIA_UI__UI_UTIL_H

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <string>

namespace aspia {

std::wstring GetWindowString(const CWindow& window);

} // namespace aspia

#endif // _ASPIA_UI__UI_UTIL_H
