//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_capturer.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__DESKTOP_CAPTURER_H_
#define ASPIA_DESKTOP_CAPTURE__DESKTOP_CAPTURER_H_

namespace aspia {

class DesktopCapturer
{
public:
    virtual ~DesktopCapturer() = default;
};

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__DESKTOP_CAPTURER_H_
