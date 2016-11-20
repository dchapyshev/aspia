/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/input_handler.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/input_handler.h"

#include <versionhelpers.h>

#include "desktop_capture/capturer_gdi.h"
#include "base/sas_injector.h"

InputHandler::InputHandler() :
    last_mouse_x_(0),
    last_mouse_y_(0),
    last_mouse_button_mask_(0),
    ctrl_pressed_(false),
    alt_pressed_(false),
    del_pressed_(false)
{
    // Nothing
}

InputHandler::~InputHandler()
{
    // Nothing
}

void InputHandler::SwitchToInputDesktop()
{
    std::unique_ptr<Desktop> input_desktop(Desktop::GetInputDesktop());

    if (input_desktop && !desktop_.IsSame(*input_desktop))
    {
        desktop_.SetThreadDesktop(input_desktop.release());
    }

    //
    // Отправляем системе уведомления о том, что она используется для
    // предотвращения включения экранной заставки, отключения монитора,
    // перехода в спящий режим и т.д.
    //
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void InputHandler::HandlePointer(const proto::PointerEvent &msg)
{
    SwitchToInputDesktop();

    // Получаем прямоугольник экрана.
    DesktopRect screen_rect = CapturerGDI::GetDesktopRect();

    // Если полученные в сообщении координаты курсора находятся в области экрана.
    if (!screen_rect.Contains(msg.x(), msg.y()))
        return;

    int32_t x = (msg.x() * 65535) / (screen_rect.width() - 1);
    int32_t y = (msg.y() * 65535) / (screen_rect.height() - 1);

    DWORD flags = MOUSEEVENTF_ABSOLUTE;
    DWORD wheel_movement = 0;
    bool update_mouse = false;

    if (x != last_mouse_x_ || y != last_mouse_y_)
    {
        flags |= MOUSEEVENTF_MOVE;

        last_mouse_x_ = x;
        last_mouse_y_ = y;

        update_mouse = true;
    }

    if ((msg.mask() & proto::PointerEvent::LEFT_BUTTON) !=
        (last_mouse_button_mask_ & proto::PointerEvent::LEFT_BUTTON))
    {
        flags |= (msg.mask() & proto::PointerEvent::LEFT_BUTTON) ?
            MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;

        update_mouse = true;
    }

    if ((msg.mask() & proto::PointerEvent::MIDDLE_BUTTON) !=
        (last_mouse_button_mask_ & proto::PointerEvent::MIDDLE_BUTTON))
    {
        flags |= (msg.mask() & proto::PointerEvent::MIDDLE_BUTTON) ?
            MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;

        update_mouse = true;
    }

    if ((msg.mask() & proto::PointerEvent::RIGHT_BUTTON) !=
        (last_mouse_button_mask_ & proto::PointerEvent::RIGHT_BUTTON))
    {
        flags |= (msg.mask() & proto::PointerEvent::RIGHT_BUTTON) ?
            MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;

        update_mouse = true;
    }

    if (msg.mask() & proto::PointerEvent::WHEEL_UP)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(WHEEL_DELTA);
        update_mouse = true;
    }

    if (msg.mask() & proto::PointerEvent::WHEEL_DOWN)
    {
        flags |= MOUSEEVENTF_WHEEL;
        wheel_movement = static_cast<DWORD>(-WHEEL_DELTA);
        update_mouse = true;
    }

    if (update_mouse)
    {
        INPUT input = { 0 };

        input.type         = INPUT_MOUSE;
        input.mi.dx        = x;
        input.mi.dy        = y;
        input.mi.mouseData = wheel_movement;
        input.mi.dwFlags   = flags;

        // Do the mouse event
        SendInput(1, &input, sizeof(input));

        last_mouse_button_mask_ = msg.mask();
    }
}

void InputHandler::HandleCAD()
{
    // Windows Vista/7/8/10 (2008/2008R2/2012/2016)
    if (IsWindowsVistaOrGreater())
    {
        std::unique_ptr<SasInjector> sas_injector(new SasInjector());

        sas_injector->InjectSAS();
    }
    // Windows XP/2003
    else
    {
        const WCHAR kWinlogonDesktopName[] = L"Winlogon";
        const WCHAR kSasWindowClassName[] = L"SAS window class";
        const WCHAR kSasWindowTitle[] = L"SAS window";

        std::unique_ptr<Desktop> winlogon_desktop(Desktop::GetDesktop(kWinlogonDesktopName));

        if (winlogon_desktop)
        {
            ScopedThreadDesktop desktop;

            if (desktop.SetThreadDesktop(winlogon_desktop.release()))
            {
                HWND window = FindWindowW(kSasWindowClassName, kSasWindowTitle);

                if (!window)
                    window = HWND_BROADCAST;

                PostMessageW(window,
                             WM_HOTKEY,
                             0,
                             MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));
            }
        }
    }
}

void InputHandler::HandleKeyboard(const proto::KeyEvent &msg)
{
    if (msg.keycode() == VK_CONTROL && !msg.extended())
        ctrl_pressed_ = msg.pressed();

    if (msg.keycode() == VK_MENU && !msg.extended())
        alt_pressed_ = msg.pressed();

    if (msg.keycode() == VK_DELETE && msg.extended())
        del_pressed_ = msg.pressed();

    if (ctrl_pressed_ && alt_pressed_ && del_pressed_)
    {
        HandleCAD();
    }
    else
    {
        SwitchToInputDesktop();

        DWORD flags = (msg.extended() ? KEYEVENTF_EXTENDEDKEY : 0);

        INPUT input = { 0 };

        input.type       = INPUT_KEYBOARD;
        input.ki.wVk     = msg.keycode();
        input.ki.dwFlags = flags | (msg.pressed() ? 0 : KEYEVENTF_KEYUP);
        input.ki.wScan   = static_cast<WORD>(MapVirtualKeyW(msg.keycode(), MAPVK_VK_TO_VSC));

        // Do the keyboard event
        SendInput(1, &input, sizeof(input));
    }
}
