/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/input_event.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/input_handler.h"

InputHandler::InputHandler() :
    last_mouse_x_(0),
    last_mouse_y_(0),
    last_mouse_button_mask_(0)
{

}

InputHandler::~InputHandler()
{

}

void InputHandler::ExecuteMouse(int x, int y, uint8_t button_mask)
{
    DWORD flags = MOUSEEVENTF_ABSOLUTE;
    DWORD wheel_movement = 0;
    bool update_mouse;

    if (x != last_mouse_x_ || y != last_mouse_y_)
    {
        flags |= MOUSEEVENTF_MOVE;

        last_mouse_x_ = x;
        last_mouse_y_ = y;

        update_mouse = true;
    }

    if ((button_mask & kLeftButtonMask) != (last_mouse_button_mask_ & kLeftButtonMask))
    {
        flags |= (button_mask & kLeftButtonMask) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        update_mouse = true;
    }

    if ((button_mask & kMiddleButtonMask) != (last_mouse_button_mask_ & kMiddleButtonMask))
    {
        flags |= (button_mask & kMiddleButtonMask) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        update_mouse = true;
    }

    if ((button_mask & kRightButtonMask) != (last_mouse_button_mask_ & kRightButtonMask))
    {
        flags |= (button_mask & kRightButtonMask) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        update_mouse = true;
    }

    if (button_mask & kWheelUpMask)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(WHEEL_DELTA);
        update_mouse = true;
    }

    if (button_mask & kWheelDownMask)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
        update_mouse = true;
    }

    if (update_mouse)
    {
        INPUT input;

        input.type           = INPUT_MOUSE;
        input.mi.dx          = x;
        input.mi.dy          = y;
        input.mi.mouseData   = wheel_movement;
        input.mi.dwFlags     = flags;
        input.mi.time        = 0;
        input.mi.dwExtraInfo = 0;

        // Do the mouse event
        SendInput(1, &input, sizeof(INPUT));

        last_mouse_button_mask_ = button_mask;
    }
}

void InputHandler::ExecuteKeyboard(uint8_t vk_key_code, bool extended, bool down)
{
    uint16_t flags = (extended ? KEYEVENTF_EXTENDEDKEY : 0);

    INPUT input;

    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = vk_key_code;
    input.ki.dwFlags     = flags | (down ? 0 : KEYEVENTF_KEYUP);
    input.ki.wScan       = static_cast<WORD>(MapVirtualKeyW(vk_key_code, MAPVK_VK_TO_VSC));
    input.ki.time        = 0;
    input.ki.dwExtraInfo = 0;

    // Do the keyboard event
    SendInput(1, &input, sizeof(INPUT));
}
