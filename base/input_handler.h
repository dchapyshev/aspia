/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/input_handler.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__INPUT_HANDLER_H
#define _ASPIA_BASE__INPUT_HANDLER_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/scoped_thread_desktop.h"
#include "proto/proto.pb.h"

class InputHandler
{
public:
    InputHandler();
    ~InputHandler();

    void HandlePointer(const proto::PointerEvent &msg);
    void HandleKeyboard(const proto::KeyEvent &msg);

private:
    void SwitchToInputDesktop();
    void HandleCAD();

private:
    ScopedThreadDesktop desktop_;

    int32_t last_mouse_x_;
    int32_t last_mouse_y_;
    int32_t last_mouse_button_mask_;

    bool ctrl_pressed_;
    bool alt_pressed_;
    bool del_pressed_;

    DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

#endif // _ASPIA_BASE__INPUT_HANDLER_H
