//
// PROJECT:         Aspia
// FILE:            host/input_injector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include "base/scoped_thread_desktop.h"
#include "proto/desktop_session.pb.h"
#include "desktop_capture/desktop_geometry.h"

namespace aspia {

class InputInjector
{
public:
    InputInjector() = default;
    ~InputInjector() = default;

    // The calling thread should not own any windows.
    void InjectPointerEvent(const proto::desktop::PointerEvent& event);
    void InjectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    void SwitchToInputDesktop();

    ScopedThreadDesktop desktop_;

    DesktopPoint prev_mouse_pos_;
    uint32_t prev_mouse_button_mask_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
