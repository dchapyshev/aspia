//
// PROJECT:         Aspia
// FILE:            host/input_injector.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include <set>

#include "base/scoped_thread_desktop.h"
#include "desktop_capture/desktop_geometry.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class InputInjector
{
public:
    InputInjector() = default;
    ~InputInjector();

    void InjectPointerEvent(const proto::desktop::PointerEvent& event);
    void InjectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    void SwitchToInputDesktop();
    bool IsCtrlAndAltPressed();

    ScopedThreadDesktop desktop_;

    std::set<uint32_t> pressed_keys_;

    DesktopPoint prev_mouse_pos_;
    uint32_t prev_mouse_button_mask_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
