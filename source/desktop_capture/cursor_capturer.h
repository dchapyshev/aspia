//
// PROJECT:         Aspia
// FILE:            desktop_capture/cursor_capturer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H_
#define ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H_

namespace aspia {

class MouseCursor;

class CursorCapturer
{
public:
    virtual ~CursorCapturer() = default;

    virtual MouseCursor* captureCursor() = 0;
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__CURSOR_CAPTURER_H_
