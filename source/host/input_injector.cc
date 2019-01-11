//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/input_injector.h"

#include <QRect>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sas.h>

#include <set>

#include "base/win/registry.h"
#include "base/logging.h"
#include "base/keycode_converter.h"
#include "desktop_capture/win/scoped_thread_desktop.h"

namespace aspia {

namespace {

const wchar_t kSoftwareSASGenerationPath[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";

const wchar_t kSoftwareSASGeneration[] = L"SoftwareSASGeneration";

const uint32_t kUsbCodeDelete = 0x07004c;
const uint32_t kUsbCodeLeftCtrl = 0x0700e0;
const uint32_t kUsbCodeRightCtrl = 0x0700e4;
const uint32_t kUsbCodeLeftAlt = 0x0700e2;
const uint32_t kUsbCodeRightAlt = 0x0700e6;

void sendKeyboardScancode(WORD scancode, DWORD flags)
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

void sendKeyboardVirtualKey(WORD key_code, DWORD flags)
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

class InputInjectorImpl
{
public:
    InputInjectorImpl(bool block_input);
    ~InputInjectorImpl();

    void injectPointerEvent(const proto::desktop::PointerEvent& event);
    void injectKeyEvent(const proto::desktop::KeyEvent& event);

private:
    void switchToInputDesktop();
    void injectSAS();
    bool isCtrlAndAltPressed();

    ScopedThreadDesktop desktop_;

    const bool block_input_;

    std::set<uint32_t> pressed_keys_;
    QPoint prev_mouse_pos_;

    uint32_t prev_mouse_button_mask_ = 0;

    DISALLOW_COPY_AND_ASSIGN(InputInjectorImpl);
};

InputInjectorImpl::InputInjectorImpl(bool block_input)
    : block_input_(block_input)
{
    switchToInputDesktop();

    if (block_input_)
        BlockInput(TRUE);
}

InputInjectorImpl::~InputInjectorImpl()
{
    for (auto usb_keycode : pressed_keys_)
    {
        int scancode = KeycodeConverter::usbKeycodeToNativeKeycode(usb_keycode);
        if (scancode != KeycodeConverter::invalidNativeKeycode())
        {
            sendKeyboardScancode(static_cast<WORD>(scancode),
                                 KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP);
        }
    }
}

void InputInjectorImpl::injectPointerEvent(const proto::desktop::PointerEvent& event)
{
    switchToInputDesktop();

    QRect screen_rect(GetSystemMetrics(SM_XVIRTUALSCREEN),
                      GetSystemMetrics(SM_YVIRTUALSCREEN),
                      GetSystemMetrics(SM_CXVIRTUALSCREEN),
                      GetSystemMetrics(SM_CYVIRTUALSCREEN));
    if (screen_rect.isEmpty())
        return;

    // Translate the coordinates of the cursor into the coordinates of the virtual screen.
    QPoint pos(((event.x() - screen_rect.x()) * 65535) / (screen_rect.width() - 1),
               ((event.y() - screen_rect.y()) * 65535) / (screen_rect.height() - 1));

    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    DWORD wheel_movement = 0;

    if (pos != prev_mouse_pos_)
    {
        flags |= MOUSEEVENTF_MOVE;
        prev_mouse_pos_ = pos;
    }

    uint32_t mask = event.mask();

    // If the host is configured to swap left & right buttons.
    bool swap_buttons = !!GetSystemMetrics(SM_SWAPBUTTON);

    bool prev = (prev_mouse_button_mask_ & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;
    bool curr = (mask & proto::desktop::PointerEvent::LEFT_BUTTON) != 0;
    if (curr != prev)
    {
        if (!swap_buttons)
            flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        else
            flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
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
        if (!swap_buttons)
            flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        else
            flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
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

void InputInjectorImpl::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    if (event.flags() & proto::desktop::KeyEvent::PRESSED)
    {
        pressed_keys_.insert(event.usb_keycode());

        if (event.usb_keycode() == kUsbCodeDelete && isCtrlAndAltPressed())
        {
            injectSAS();
            return;
        }
    }
    else
    {
        pressed_keys_.erase(event.usb_keycode());
    }

    int scancode = KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (scancode == KeycodeConverter::invalidNativeKeycode())
        return;

    switchToInputDesktop();

    bool prev_state = GetKeyState(VK_CAPITAL) != 0;
    bool curr_state = (event.flags() & proto::desktop::KeyEvent::CAPSLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_CAPITAL, 0);
        sendKeyboardVirtualKey(VK_CAPITAL, KEYEVENTF_KEYUP);
    }

    prev_state = GetKeyState(VK_NUMLOCK) != 0;
    curr_state = (event.flags() & proto::desktop::KeyEvent::NUMLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_NUMLOCK, 0);
        sendKeyboardVirtualKey(VK_NUMLOCK, KEYEVENTF_KEYUP);
    }

    DWORD flags = KEYEVENTF_SCANCODE;

    if (!(event.flags() & proto::desktop::KeyEvent::PRESSED))
        flags |= KEYEVENTF_KEYUP;

    sendKeyboardScancode(static_cast<WORD>(scancode), flags);
}

void InputInjectorImpl::switchToInputDesktop()
{
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
        desktop_.setThreadDesktop(std::move(input_desktop));

    if (block_input_)
        BlockInput(TRUE);

    // We send a notification to the system that it is used to prevent
    // the screen saver, going into hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

void InputInjectorImpl::injectSAS()
{
    static const DWORD kNone = 0;
    static const DWORD kApplications = 2;

    RegistryKey key;
    DWORD old_state = kNone;

    LONG status = key.create(HKEY_LOCAL_MACHINE, kSoftwareSASGenerationPath, KEY_READ | KEY_WRITE);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "key.create failed" << systemErrorCodeToString(status);
    }
    else
    {
        status = key.readValueDW(kSoftwareSASGeneration, &old_state);
        if (status != ERROR_SUCCESS)
        {
            // The previous state is not defined.
            // Consider that the software generation of SAS has been disabled.
            old_state = kNone;
        }

        if (old_state < kApplications)
        {
            status = key.writeValue(kSoftwareSASGeneration, kApplications);
            if (status != ERROR_SUCCESS)
            {
                LOG(LS_WARNING) << "key.writeValue failed" << systemErrorCodeToString(status);
                key.close();
            }
        }
    }

    SendSAS(FALSE);

    if (key.isValid())
    {
        status = key.writeValue(kSoftwareSASGeneration, old_state);
        if (status != ERROR_SUCCESS)
        {
            LOG(LS_WARNING) << "key.writeValue failed" << systemErrorCodeToString(status);
        }
    }
}

bool InputInjectorImpl::isCtrlAndAltPressed()
{
    bool ctrl_pressed = false;
    bool alt_pressed = false;

    for (const auto& key : pressed_keys_)
    {
        if (key == kUsbCodeLeftCtrl || key == kUsbCodeRightCtrl)
            ctrl_pressed = true;

        if (key == kUsbCodeLeftAlt || key == kUsbCodeRightAlt)
            alt_pressed = true;
    }

    return ctrl_pressed && alt_pressed;
}

} // namespace

InputInjector::InputInjector(QObject* parent, bool block_input)
    : QThread(parent),
      block_input_(block_input)
{
    start(QThread::HighPriority);
}

InputInjector::~InputInjector()
{
    {
        std::scoped_lock lock(input_queue_lock_);
        terminate_ = true;
        input_event_.notify_one();
    }

    wait();
}

void InputInjector::injectPointerEvent(const proto::desktop::PointerEvent& event)
{
    std::scoped_lock lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputInjector::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    std::scoped_lock lock(input_queue_lock_);
    incoming_input_queue_.emplace(event);
    input_event_.notify_one();
}

void InputInjector::run()
{
    InputInjectorImpl impl(block_input_);

    while (true)
    {
        std::queue<InputEvent> work_input_queue;

        {
            std::unique_lock lock(input_queue_lock_);

            while (incoming_input_queue_.empty() && !terminate_)
                input_event_.wait(lock);

            if (terminate_)
                return;

            work_input_queue.swap(incoming_input_queue_);
        }

        while (!work_input_queue.empty())
        {
            const InputEvent& input_event = work_input_queue.front();

            if (std::holds_alternative<proto::desktop::KeyEvent>(input_event))
            {
                impl.injectKeyEvent(std::get<proto::desktop::KeyEvent>(input_event));
            }
            else if (std::holds_alternative<proto::desktop::PointerEvent>(input_event))
            {
                impl.injectPointerEvent(std::get<proto::desktop::PointerEvent>(input_event));
            }

            work_input_queue.pop();
        }
    }
}

} // namespace aspia
