//
// PROJECT:         Aspia
// FILE:            host/input_injector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__INPUT_INJECTOR_H
#define _ASPIA_HOST__INPUT_INJECTOR_H

#include <set>

#include "base/threading/thread.h"
#include "desktop_capture/desktop_geometry.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class ScopedThreadDesktop;

class InputInjector : private Thread::Delegate
{
public:
    InputInjector();
    ~InputInjector();

    void InjectPointerEvent(const proto::desktop::PointerEvent& event);
    void InjectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void SwitchToInputDesktop();
    bool IsCtrlAndAltPressed();

    Thread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    std::unique_ptr<ScopedThreadDesktop> desktop_;

    std::set<uint32_t> pressed_keys;

    DesktopPoint prev_mouse_pos_;
    uint32_t prev_mouse_button_mask_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InputInjector);
};

} // namespace aspia

#endif // _ASPIA_HOST__INPUT_INJECTOR_H
