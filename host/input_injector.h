//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/input_injector.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/scoped_thread_desktop.h"
#include "proto/proto.pb.h"
#include "desktop_capture/desktop_point.h"

namespace aspia {

class InputInjector
{
public:
    InputInjector();
    ~InputInjector() = default;

    void InjectPointerEvent(const proto::PointerEvent& event);
    void InjectKeyEvent(const proto::KeyEvent& event);

private:
    void SwitchToInputDesktop();
    void SendKeyboardInput(WORD key_code, DWORD flags);

private:
    ScopedThreadDesktop desktop_;

    DesktopPoint prev_mouse_pos_;
    uint32_t prev_mouse_button_mask_;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
