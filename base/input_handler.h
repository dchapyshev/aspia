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

class InputHandler
{
public:
    InputHandler();
    ~InputHandler();

    static const uint8_t kLeftButtonMask   = 1;
    static const uint8_t kMiddleButtonMask = 2;
    static const uint8_t kRightButtonMask  = 4;
    static const uint8_t kWheelUpMask      = 8;
    static const uint8_t kWheelDownMask    = 16;

    void ExecuteMouse(int x, int y, uint8_t button_mask);
    void ExecuteKeyboard(uint8_t vk_key_code, bool extended, bool down);

private:
    int last_mouse_x_;
    int last_mouse_y_;
    uint8_t last_mouse_button_mask_;
};

#endif // _ASPIA_BASE__INPUT_HANDLER_H
