//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/input_injector.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/input_injector.h"

#include "base/logging.h"
#include "desktop_capture/desktop_rect.h"
#include "host/sas_injector.h"

namespace aspia {

InputInjector::InputInjector() :
    prev_mouse_button_mask_(0)
{
    // Nothing
}

void InputInjector::SwitchToInputDesktop()
{
    Desktop input_desktop(Desktop::GetInputDesktop());

    if (input_desktop.IsValid() && !desktop_.IsSame(input_desktop))
    {
        desktop_.SetThreadDesktop(std::move(input_desktop));
    }

    //
    // Отправляем системе уведомления о том, что она используется для
    // предотвращения включения экранной заставки, отключения монитора,
    // перехода в спящий режим и т.д.
    //
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void InputInjector::InjectPointerEvent(const proto::PointerEvent& event)
{
    SwitchToInputDesktop();

    // Получаем прямоугольник экрана.
    DesktopRect screen_rect = DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_YVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_CYVIRTUALSCREEN));

    // Если полученные в сообщении координаты курсора не находятся в области экрана.
    if (!screen_rect.Contains(event.x(), event.y()))
    {
        // Выходим.
        return;
    }

    // Переводим координаты курсора в координаты виртуального экрана.
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

    bool prev = (prev_mouse_button_mask_ & proto::PointerEvent::LEFT_BUTTON) != 0;
    bool curr = (mask & proto::PointerEvent::LEFT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    }

    prev = (prev_mouse_button_mask_ & proto::PointerEvent::MIDDLE_BUTTON) != 0;
    curr = (mask & proto::PointerEvent::MIDDLE_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
    }

    prev = (prev_mouse_button_mask_ & proto::PointerEvent::RIGHT_BUTTON) != 0;
    curr = (mask & proto::PointerEvent::RIGHT_BUTTON) != 0;

    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    }

    if (mask & proto::PointerEvent::WHEEL_UP)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(WHEEL_DELTA);
    }
    else if (mask & proto::PointerEvent::WHEEL_DOWN)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
    }

    INPUT input;

    input.type           = INPUT_MOUSE;
    input.mi.dx          = pos.x();
    input.mi.dy          = pos.y();
    input.mi.mouseData   = wheel_movement;
    input.mi.dwFlags     = flags;
    input.mi.time        = 0;
    input.mi.dwExtraInfo = 0;

    // Do the mouse event
    if (!SendInput(1, &input, sizeof(input)))
    {
        DLOG(WARNING) << "SendInput() failed: " << GetLastError();
    }

    prev_mouse_button_mask_ = mask;
}

void InputInjector::SendKeyboardInput(WORD key_code, DWORD flags)
{
    INPUT input;

    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = key_code;
    input.ki.dwFlags     = flags;
    input.ki.wScan       = static_cast<WORD>(MapVirtualKeyW(key_code, MAPVK_VK_TO_VSC));
    input.ki.time        = 0;
    input.ki.dwExtraInfo = 0;

    // Do the keyboard event
    if (!SendInput(1, &input, sizeof(input)))
    {
        DLOG(WARNING) << "SendInput() failed: " << GetLastError();
    }
}

void InputInjector::InjectKeyEvent(const proto::KeyEvent& event)
{
    SwitchToInputDesktop();

    // Если нажата комбинация Ctrl + Alt + Delete.
    if ((event.flags() & proto::KeyEvent::PRESSED) && event.keycode() == VK_DELETE &&
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000))
    {
        SasInjector::InjectSAS();
        return;
    }

    bool prev_state = GetKeyState(VK_CAPITAL) != 0;
    bool curr_state = (event.flags() & proto::KeyEvent::CAPSLOCK) != 0;

    if (prev_state != curr_state)
    {
        SendKeyboardInput(VK_CAPITAL, 0);
        SendKeyboardInput(VK_CAPITAL, KEYEVENTF_KEYUP);
    }

    prev_state = GetKeyState(VK_NUMLOCK) != 0;
    curr_state = (event.flags() & proto::KeyEvent::NUMLOCK) != 0;

    if (prev_state != curr_state)
    {
        SendKeyboardInput(VK_NUMLOCK, 0);
        SendKeyboardInput(VK_NUMLOCK, KEYEVENTF_KEYUP);
    }

    DWORD flags = 0;

    flags |= ((event.flags() & proto::KeyEvent::EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0);
    flags |= ((event.flags() & proto::KeyEvent::PRESSED) ? 0 : KEYEVENTF_KEYUP);

    SendKeyboardInput(event.keycode(), flags);
}

} // namespace aspia
