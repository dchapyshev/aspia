//
// PROJECT:         Aspia
// FILE:            host/input_injector.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/input_injector.h"

#include "base/scoped_thread_desktop.h"
#include "base/logging.h"
#include "host/sas_injector.h"
#include "protocol/keycode_converter.h"

namespace aspia {

namespace {

const uint32_t kUsbCodeDelete = 0x07004c;
const uint32_t kUsbCodeLeftCtrl = 0x0700e0;
const uint32_t kUsbCodeRightCtrl = 0x0700e4;
const uint32_t kUsbCodeLeftAlt = 0x0700e2;
const uint32_t kUsbCodeRightAlt = 0x0700e6;

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

InputInjector::InputInjector()
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

InputInjector::~InputInjector()
{
    thread_.Stop();
}

void InputInjector::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);

    desktop_ = std::make_unique<ScopedThreadDesktop>();
}

void InputInjector::OnAfterThreadRunning()
{
    for (auto iter = pressed_keys.begin(); iter != pressed_keys.end(); ++iter)
    {
        int scancode = KeycodeConverter::UsbKeycodeToNativeKeycode(*iter);
        if (scancode != KeycodeConverter::InvalidNativeKeycode())
        {
            SendKeyboardScancode(static_cast<WORD>(scancode),
                                 KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP);
        }
    }
}

void InputInjector::SwitchToInputDesktop()
{
    Desktop input_desktop(Desktop::GetInputDesktop());

    if (input_desktop.IsValid() && !desktop_->IsSame(input_desktop))
    {
        desktop_->SetThreadDesktop(std::move(input_desktop));
    }

    // We send a notification to the system that it is used to prevent
    // the screen saver, going into hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void InputInjector::InjectPointerEvent(const proto::desktop::PointerEvent& event)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&InputInjector::InjectPointerEvent, this, event));
        return;
    }

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
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&InputInjector::InjectKeyEvent, this, event));
        return;
    }

    if (event.flags() & proto::desktop::KeyEvent::PRESSED)
    {
        pressed_keys.insert(event.usb_keycode());

        if (event.usb_keycode() == kUsbCodeDelete && IsCtrlAndAltPressed())
        {
            SasInjector::InjectSAS();
            return;
        }
    }
    else
    {
        pressed_keys.erase(event.usb_keycode());
    }

    int scancode = KeycodeConverter::UsbKeycodeToNativeKeycode(event.usb_keycode());
    if (scancode == KeycodeConverter::InvalidNativeKeycode())
        return;

    SwitchToInputDesktop();

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

bool InputInjector::IsCtrlAndAltPressed()
{
    size_t ctrl_keys = pressed_keys.count(kUsbCodeLeftCtrl) +
                       pressed_keys.count(kUsbCodeRightCtrl);

    size_t alt_keys = pressed_keys.count(kUsbCodeLeftAlt) +
                      pressed_keys.count(kUsbCodeRightAlt);

    return ctrl_keys != 0 && alt_keys != 0;
}

} // namespace aspia
