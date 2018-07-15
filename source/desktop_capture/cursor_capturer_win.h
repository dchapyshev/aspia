//
// PROJECT:         Aspia
// FILE:            desktop_capture/cursor_capturer_win.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_WIN_H_
#define ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_WIN_H_

#include "desktop_capture/cursor_capturer.h"

#include <qt_windows.h>
#include <memory>

namespace aspia {

class ScopedGetDC;

class CursorCapturerWin : public CursorCapturer
{
public:
    CursorCapturerWin();
    ~CursorCapturerWin() = default;

    MouseCursor* captureCursor() override;

private:
    std::unique_ptr<ScopedGetDC> desktop_dc_;
    CURSORINFO prev_cursor_info_;

    Q_DISABLE_COPY(CursorCapturerWin)
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_WIN_H_
