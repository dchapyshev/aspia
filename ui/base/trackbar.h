//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/trackbar.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__TRACKBAR_H
#define _ASPIA_UI__BASE__TRACKBAR_H

#include <commctrl.h>

namespace aspia {

static INLINE void TrackBar_SetRange(HWND trackbar, SHORT min, SHORT max)
{
    SendMessageW(trackbar, TBM_SETRANGE, TRUE, MAKELPARAM(min, max));
}

static INLINE void TrackBar_SetPos(HWND trackbar, SHORT pos)
{
    SendMessageW(trackbar, TBM_SETPOS, TRUE, pos);
}

static INLINE DWORD TrackBar_GetPos(HWND trackbar)
{
    return static_cast<DWORD>(SendMessageW(trackbar, TBM_GETPOS, 0, 0));
}

} // namespace aspia

#endif // _ASPIA_UI__BASE__TRACKBAR_H
