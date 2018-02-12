//
// PROJECT:         Aspia
// FILE:            ui/ui_util.h
// LICENSE:         Mozilla Public License Version 2.0
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

enum class LinkTypeKey { SITE, HELP, DONATE };

void OpenLink(LinkTypeKey key);

} // namespace aspia

#endif // _ASPIA_UI__UI_UTIL_H
