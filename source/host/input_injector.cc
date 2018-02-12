//
// PROJECT:         Aspia
// FILE:            host/input_injector.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/input_injector.h"

#include "base/logging.h"
#include "host/sas_injector.h"
#include "protocol/keycode_converter.h"

namespace aspia {

namespace {

void SendKeyboardScancode(WORD scancode, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type       = INPUT_KEYBOARD;
    input.ki.dwFlags = flags;
    input.ki.wScan   = scancode;

    if (!(flags & KEYEVENTF_UNICODE))
    {
        input.ki.wScan &= 0xFF;

        if ((scancode & 0xFF00) != 0x0000)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    // Do the keyboard event.
    if (!SendInput(1, &input, sizeof(input)))
        PLOG(LS_WARNING) << "SendInput failed";
}

void SendKeyboardVirtualKey(WORD key_code, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type       = INPUT_KEYBOARD;
    input.ki.wVk     = key_code;
    input.ki.dwFlags = flags;
    input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));

    // Do the keyboard event.
    if (!SendInput(1, &input, sizeof(input)))
        PLOG(LS_WARNING) << "SendInput failed";
}

} // namespace

void InputInjector::SwitchToInputDesktop()
{
    Desktop input_desktop(Desktop::GetInputDesktop());

    if (input_desktop.IsValid() && !desktop_.IsSame(input_desktop))
    {
        desktop_.SetThreadDesktop(std::move(input_desktop));
    }

    // We send a notification to the system that it is used to prevent
    // the screen saver, going into hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void InputInjector::InjectPointerEvent(const proto::desktop::PointerEvent& event)
{
    SwitchToInputDesktop();

    DesktopRect screen_rect =
        DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                              GetSystemMetrics(SM_YVIRTUALSCREEN),
                              GetSystemMetrics(SM_CXVIRTUALSCREEN),
                              GetSystemMetrics(SM_CYVIRTUALSCREEN));
    if (!screen_rect.Contains(event.x(), event.y()))
        return;

    // Translate the coordinates of the cursor into the coordinates of the virtual screen.
    DesktopPoint pos(((event.x() - screen_rect.x()) * 65535) / (screen_rect.Width() - 1),
                     ((event.y() - screen_rect.y()) * 65535) / (screen_rect.Height() - 1));

    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    DWORD wheel_movement = 0;

    if (!pos.IsEqual(prev_mouse_pos_))
    {
        flags |= MOUSEEVENTF_MOVE;
        prev_mouse_pos_ = pos;
    }

    uint32_t mask = event.mask();

    bool prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;
    bool curr = (mask & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    }

    prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::MIDDLE_BUTTON) != 0;
    curr = (mask & proto::desktop::PointerEvent::MIDDLE_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
    }

    prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::RIGHT_BUTTON) != 0;
    curr = (mask & proto::desktop::PointerEvent::RIGHT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    }

    if (mask & proto::desktop::PointerEvent::WHEEL_UP)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(WHEEL_DELTA);
    }
    else if (mask & proto::desktop::PointerEvent::WHEEL_DOWN)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
    }

    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type         = INPUT_MOUSE;
    input.mi.dx        = pos.x();
    input.mi.dy        = pos.y();
    input.mi.mouseData = wheel_movement;
    input.mi.dwFlags   = flags;

    // Do the mouse event.
    if (!SendInput(1, &input, sizeof(input)))
        PLOG(LS_WARNING) << "SendInput failed";

    prev_mouse_button_mask_ = mask;
}

void InputInjector::InjectKeyEvent(const proto::desktop::KeyEvent& event)
{
    SwitchToInputDesktop();

    const uint32_t kUsbCodeDelete = 0x07004c;

    // If the combination Ctrl + Alt + Delete is pressed.
    if ((event.flags() & proto::desktop::KeyEvent::PRESSED) && event.usb_keycode() &&
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000))
    {
        SasInjector::InjectSAS();
        return;
    }

    int scancode = KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());
    if (scancode == KeycodeConverter::InvalidNativeKeycode())
        return;

    bool prev_state = GetKeyState(VK_CAPITAL) != 0;
    bool curr_state = (event.flags() & proto::desktop::KeyEvent::CAPSLOCK) != 0;

    if (prev_state != curr_state)
    {
        SendKeyboardVirtualKey(VK_CAPITAL, 0);
        SendKeyboardVirtualKey(VK_CAPITAL, KEYEVENTF_KEYUP);
    }

    prev_state = GetKeyState(VK_NUMLOCK) != 0;
    curr_state = (event.flags() & proto::desktop::KeyEvent::NUMLOCK) != 0;

    if (prev_state != curr_state)
    {
        SendKeyboardVirtualKey(VK_NUMLOCK, 0);
        SendKeyboardVirtualKey(VK_NUMLOCK, KEYEVENTF_KEYUP);
    }

    DWORD flags = KEYEVENTF_SCANCODE;

    flags |= ((event.flags() & proto::desktop::KeyEvent::PRESSED) ? 0 : KEYEVENTF_KEYUP);

    SendKeyboardScancode(static_cast<WORD>(scancode), flags);
}

} // namespace aspia
