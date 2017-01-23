//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/input_injecotr.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/input_injector.h"

#include <versionhelpers.h>
#include <future>

#include "desktop_capture/desktop_rect.h"
#include "host/sas_injector.h"

namespace aspia {

InputInjector::InputInjector() :
    prev_mouse_button_mask_(0)
{
    bell_ = false;
}

InputInjector::~InputInjector()
{
    // Nothing
}

void InputInjector::SwitchToInputDesktop()
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

void InputInjector::InjectPointer(const proto::PointerEvent &msg)
{
    SwitchToInputDesktop();

    // Получаем прямоугольник экрана.
    DesktopRect screen_rect = DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_YVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                                    GetSystemMetrics(SM_CYVIRTUALSCREEN));

    // Если полученные в сообщении координаты курсора не находятся в области экрана.
    if (!screen_rect.Contains(msg.x(), msg.y()))
    {
        // Выходим.
        return;
    }

    // Переводим координаты курсора в координаты виртуального экрана.
    DesktopPoint pos(((msg.x() - screen_rect.x()) * 65535) / (screen_rect.Width() - 1),
                     ((msg.y() - screen_rect.y()) * 65535) / (screen_rect.Height() - 1));

    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    DWORD wheel_movement = 0;

    if (pos != prev_mouse_pos_)
    {
        flags |= MOUSEEVENTF_MOVE;
        prev_mouse_pos_ = pos;
    }

    uint32_t mask = msg.mask();

    //
    // Если на компьютере, к которому осуществляется подключение, левая и правая
    // кнопка мыши поменяны местами, то обмениваем их обратно.
    // Это необходимо для того, чтобы удаленный клиент использовал свои параметры,
    // а не параметры управляемого компьютера.
    //
    if (GetSystemMetrics(SM_SWAPBUTTON))
    {
        bool left = (mask & proto::PointerEvent::LEFT_BUTTON) != 0;
        bool right = (mask & proto::PointerEvent::RIGHT_BUTTON) != 0;

        mask = mask & ~(proto::PointerEvent::LEFT_BUTTON | proto::PointerEvent::RIGHT_BUTTON);

        if (left) mask |= proto::PointerEvent::RIGHT_BUTTON;
        if (right) mask |= proto::PointerEvent::LEFT_BUTTON;
    }

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
    SendInput(1, &input, sizeof(input));

    prev_mouse_button_mask_ = mask;
}

void InputInjector::InjectCtrlAltDel()
{
#if (_WIN32_WINNT < 0x0600)
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
#else
    std::unique_ptr<SasInjector> sas_injector(new SasInjector());

    sas_injector->InjectSAS();
#endif // (_WIN32_WINNT < 0x0600)
}

void InputInjector::InjectKeyboard(const proto::KeyEvent &msg)
{
    // Если нажата комбинация Ctrl + Alt + Delete.
    if ((msg.flags() & proto::KeyEvent::PRESSED) && msg.keycode() == VK_DELETE &&
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_MENU) & 0x8000))
    {
        InjectCtrlAltDel();
        return;
    }

    SwitchToInputDesktop();

    DWORD flags = 0;

    flags |= ((msg.flags() & proto::KeyEvent::EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0);
    flags |= ((msg.flags() & proto::KeyEvent::PRESSED) ? 0 : KEYEVENTF_KEYUP);

    INPUT input;

    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = msg.keycode();
    input.ki.dwFlags     = flags;
    input.ki.wScan       = static_cast<WORD>(MapVirtualKeyW(msg.keycode(), MAPVK_VK_TO_VSC));
    input.ki.time        = 0;
    input.ki.dwExtraInfo = 0;

    // Do the keyboard event
    SendInput(1, &input, sizeof(input));
}

void InputInjector::DoBell()
{
    bell_ = true;

    int count = 3;

    while (count-- > 0)
    {
        Beep(1200, 40);
        Beep(800, 80);
    }

    bell_ = false;
}

void InputInjector::InjectBell(const proto::BellEvent &msg)
{
    if (!bell_)
    {
        std::async(std::launch::async, &InputInjector::DoBell, this);
    }
}

} // namespace aspia
