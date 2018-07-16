//
// PROJECT:         Aspia
// FILE:            desktop_capture/screen_capturer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_H_
#define ASPIA_DESKTOP_CAPTURE__SCREEN_CAPTURER_H_

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class ScreenCapturer
{
public:
    virtual ~ScreenCapturer() = default;

    using ScreenId = qintptr;

    struct Screen
    {
        ScreenId id;
        QString title;
    };

    using ScreenList = QVector<Screen>;

    static const ScreenId kFullDesktopScreenId = -1;
    static const ScreenId kInvalidScreenId = -2;

    virtual bool screenList(ScreenList* screens) = 0;
    virtual bool selectScreen(ScreenId screen_id) = 0;
    virtual const DesktopFrame* captureFrame() = 0;
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__CAPTURER_H_
