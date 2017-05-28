//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/updown.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__UPDOWN_H
#define _ASPIA_UI__BASE__UPDOWN_H

#include <commctrl.h>

namespace aspia {

static INLINE void UpDown_SetRange(HWND updown, SHORT min, SHORT max)
{
    SendMessageW(updown, UDM_SETRANGE, 0, MAKELPARAM(max, min));
}

static INLINE void UpDown_SetPos(HWND updown, SHORT pos)
{
    SendMessageW(updown, UDM_SETPOS, 0, MAKELPARAM(pos, 0));
}

static INLINE DWORD UpDown_GetPos(HWND updown)
{
    return static_cast<DWORD>(SendMessageW(updown, UDM_GETPOS, 0, 0));
}

} // namespace aspia

#endif // _ASPIA_UI__BASE__UPDOWN_H
