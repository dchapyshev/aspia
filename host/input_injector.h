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
#include <atomic>

#include "base/macros.h"
#include "base/scoped_thread_desktop.h"
#include "proto/proto.pb.h"
#include "desktop_capture/desktop_point.h"

namespace aspia {

class InputInjector
{
public:
    InputInjector();
    ~InputInjector();

    void InjectPointer(const proto::PointerEvent &msg);
    void InjectKeyboard(const proto::KeyEvent &msg);
    void InjectBell(const proto::BellEvent &msg);

private:
    void SwitchToInputDesktop();
    void HandleCAD();
    void DoBell();

private:
    ScopedThreadDesktop desktop_;

    DesktopPoint prev_mouse_pos_;
    uint32_t prev_mouse_button_mask_;

    std::atomic_bool bell_;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
