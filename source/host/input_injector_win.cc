//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/input_injector_win.h"

#include "base/logging.h"
#include "common/keycode_converter.h"
#include "host/win/sas_injector.h"

#include <qt_windows.h>

namespace host {

namespace {

const quint32 kUsbCodeDelete = 0x07004c;
const quint32 kUsbCodeLeftCtrl = 0x0700e0;
const quint32 kUsbCodeRightCtrl = 0x0700e4;
const quint32 kUsbCodeLeftAlt = 0x0700e2;
const quint32 kUsbCodeRightAlt = 0x0700e6;

//--------------------------------------------------------------------------------------------------
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
    {
        PLOG(LS_ERROR) << "SendInput failed";
    }
}

//--------------------------------------------------------------------------------------------------
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
    {
        PLOG(LS_ERROR) << "SendInput failed";
    }
}

//--------------------------------------------------------------------------------------------------
void sendKeyboardUnicodeChar(WORD unicode_char, DWORD flags)
{
    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type = INPUT_KEYBOARD;
    input.ki.dwFlags = KEYEVENTF_UNICODE | flags;
    input.ki.wScan = unicode_char;

    // Do the keyboard event.
    if (!SendInput(1, &input, sizeof(input)))
    {
        PLOG(LS_ERROR) << "SendInput failed";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorWin::InputInjectorWin(QObject* parent)
    : InputInjector(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
InputInjectorWin::~InputInjectorWin()
{
    LOG(LS_INFO) << "Dtor";

    setBlockInputImpl(false);
    for (const auto& key : std::as_const(pressed_keys_))
    {
        int scancode = common::KeycodeConverter::usbKeycodeToNativeKeycode(key);
        if (scancode != common::KeycodeConverter::invalidNativeKeycode())
        {
            sendKeyboardScancode(static_cast<WORD>(scancode), KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP);
        }
        else
        {
            LOG(LS_ERROR) << "Invalid key code: " << key;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::setScreenOffset(const base::Point& offset)
{
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::setBlockInput(bool enable)
{
    setBlockInputImpl(enable);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::injectKeyEvent(const proto::KeyEvent& event)
{
    if (event.flags() & proto::KeyEvent::PRESSED)
    {
        pressed_keys_.insert(event.usb_keycode());

        if (event.usb_keycode() == kUsbCodeDelete && isCtrlAndAltPressed())
        {
            LOG(LS_INFO) << "CTRL+ALT+DEL detected";
            injectSAS();
            return;
        }
    }
    else
    {
        if (!pressed_keys_.contains(event.usb_keycode()))
        {
            LOG(LS_INFO) << "No pressed key in the list";
            return;
        }

        pressed_keys_.remove(event.usb_keycode());
    }

    int scancode = common::KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (scancode == common::KeycodeConverter::invalidNativeKeycode())
    {
        LOG(LS_ERROR) << "Invalid key code: " << event.usb_keycode();
        return;
    }

    beforeInput();

    bool prev_state = GetKeyState(VK_CAPITAL) != 0;
    bool curr_state = (event.flags() & proto::KeyEvent::CAPSLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_CAPITAL, 0);
        sendKeyboardVirtualKey(VK_CAPITAL, KEYEVENTF_KEYUP);
    }

    prev_state = GetKeyState(VK_NUMLOCK) != 0;
    curr_state = (event.flags() & proto::KeyEvent::NUMLOCK) != 0;

    if (prev_state != curr_state)
    {
        sendKeyboardVirtualKey(VK_NUMLOCK, 0);
        sendKeyboardVirtualKey(VK_NUMLOCK, KEYEVENTF_KEYUP);
    }

    DWORD flags = KEYEVENTF_SCANCODE;

    if (!(event.flags() & proto::KeyEvent::PRESSED))
        flags |= KEYEVENTF_KEYUP;

    sendKeyboardScancode(static_cast<WORD>(scancode), flags);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::injectTextEvent(const proto::TextEvent& event)
{
    QString text = QString::fromStdString(event.text());
    if (text.isEmpty())
        return;

    beforeInput();

    for (auto it = text.cbegin(); it != text.cend(); ++it)
    {
        if (*it == '\n')
        {
            // The WM_CHAR event generated for carriage return is '\r', not '\n', and some
            // applications may check for VK_RETURN explicitly, so handle newlines specially.
            sendKeyboardVirtualKey(VK_RETURN, 0);
            sendKeyboardVirtualKey(VK_RETURN, KEYEVENTF_KEYUP);
        }

        sendKeyboardUnicodeChar(it->unicode(), 0);
        sendKeyboardUnicodeChar(it->unicode(), KEYEVENTF_KEYUP);
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::injectMouseEvent(const proto::MouseEvent& event)
{
    beforeInput();

    base::Size full_size(GetSystemMetrics(SM_CXVIRTUALSCREEN),
                         GetSystemMetrics(SM_CYVIRTUALSCREEN));
    if (full_size.width() <= 1 || full_size.height() <= 1)
    {
        LOG(LS_ERROR) << "Invalid screen size: " << full_size;
        return;
    }

    // Translate the coordinates of the cursor into the coordinates of the virtual screen.
    base::Point pos(((event.x() + screen_offset_.x()) * 65535) / (full_size.width() - 1),
                    ((event.y() + screen_offset_.y()) * 65535) / (full_size.height() - 1));

    DWORD flags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    DWORD mouse_data = 0;

    if (pos != last_mouse_pos_)
    {
        flags |= MOUSEEVENTF_MOVE;
        last_mouse_pos_ = pos;
    }

    quint32 mask = event.mask();

    // If the host is configured to swap left & right buttons.
    bool swap_buttons = !!GetSystemMetrics(SM_SWAPBUTTON);

    bool prev = (last_mouse_mask_ & proto::MouseEvent::LEFT_BUTTON) != 0;
    bool curr = (mask & proto::MouseEvent::LEFT_BUTTON) != 0;
    if (curr != prev)
    {
        if (!swap_buttons)
            flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        else
            flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    }

    prev = (last_mouse_mask_ & proto::MouseEvent::MIDDLE_BUTTON) != 0;
    curr = (mask & proto::MouseEvent::MIDDLE_BUTTON) != 0;
    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
    }

    prev = (last_mouse_mask_ & proto::MouseEvent::RIGHT_BUTTON) != 0;
    curr = (mask & proto::MouseEvent::RIGHT_BUTTON) != 0;
    if (curr != prev)
    {
        if (!swap_buttons)
            flags |= (curr ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        else
            flags |= (curr ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    }

    prev = (last_mouse_mask_ & proto::MouseEvent::BACK_BUTTON) != 0;
    curr = (mask & proto::MouseEvent::BACK_BUTTON) != 0;
    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
        mouse_data = XBUTTON1;
    }

    prev = (last_mouse_mask_ & proto::MouseEvent::FORWARD_BUTTON) != 0;
    curr = (mask & proto::MouseEvent::FORWARD_BUTTON) != 0;
    if (curr != prev)
    {
        flags |= (curr ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
        mouse_data = XBUTTON2;
    }

    if (mask & proto::MouseEvent::WHEEL_UP)
    {
        flags |= MOUSEEVENTF_WHEEL;
        mouse_data = static_cast<DWORD>(WHEEL_DELTA);
    }
    else if (mask & proto::MouseEvent::WHEEL_DOWN)
    {
        flags |= MOUSEEVENTF_WHEEL;
        mouse_data = static_cast<DWORD>(-WHEEL_DELTA);
    }

    INPUT input;
    memset(&input, 0, sizeof(input));

    input.type = INPUT_MOUSE;
    input.mi.dx = pos.x();
    input.mi.dy = pos.y();
    input.mi.mouseData = mouse_data;
    input.mi.dwFlags = flags;

    // Do the mouse event.
    if (!SendInput(1, &input, sizeof(input)))
    {
        PLOG(LS_ERROR) << "SendInput failed";
    }

    last_mouse_mask_ = mask;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::injectTouchEvent(const proto::TouchEvent& event)
{
    touch_injector_.injectTouchEvent(event);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::beforeInput()
{
    BlockInput(!!block_input_);

    // We send a notification to the system that it is used to prevent
    // the screen saver, going into hibernation mode, etc.
    SetThreadExecutionState(ES_SYSTEM_REQUIRED);
}

//--------------------------------------------------------------------------------------------------
bool InputInjectorWin::isCtrlAndAltPressed()
{
    bool ctrl_pressed = false;
    bool alt_pressed = false;

    for (const auto& key : std::as_const(pressed_keys_))
    {
        if (key == kUsbCodeLeftCtrl || key == kUsbCodeRightCtrl)
            ctrl_pressed = true;

        if (key == kUsbCodeLeftAlt || key == kUsbCodeRightAlt)
            alt_pressed = true;
    }

    return ctrl_pressed && alt_pressed;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWin::setBlockInputImpl(bool enable)
{
    beforeInput();
    block_input_ = enable;
    BlockInput(!!enable);
}

} // namespace host
