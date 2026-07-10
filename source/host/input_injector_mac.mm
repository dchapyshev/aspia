//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/input_injector_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include "base/logging.h"
#include "base/mac/login_utils.h"
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"

namespace {

// USB HID usage codes of the modifier keys (see the HID Usage Tables, keyboard page).
const quint32 kUsbCodeLeftCtrl   = 0x0700e0;
const quint32 kUsbCodeLeftShift  = 0x0700e1;
const quint32 kUsbCodeLeftAlt    = 0x0700e2;
const quint32 kUsbCodeLeftGui    = 0x0700e3;
const quint32 kUsbCodeRightCtrl  = 0x0700e4;
const quint32 kUsbCodeRightShift = 0x0700e5;
const quint32 kUsbCodeRightAlt   = 0x0700e6;
const quint32 kUsbCodeRightGui   = 0x0700e7;

// Injected events are posted at the HID level, the same layer physical devices feed into. A host
// service running as root may post there without additional privileges.
const CGEventTapLocation kEventTap = kCGHIDEventTap;

//--------------------------------------------------------------------------------------------------
void postEvent(CGEventRef event)
{
    if (!event)
        return;

    CGEventPost(kEventTap, event);
    CFRelease(event);
}

//--------------------------------------------------------------------------------------------------
void postMouseEvent(CGEventType type, CGPoint position, CGMouseButton button, int click_state)
{
    CGEventRef event = CGEventCreateMouseEvent(nullptr, type, position, button);
    if (!event)
    {
        LOG(ERROR) << "CGEventCreateMouseEvent failed";
        return;
    }

    // A non-zero click state distinguishes a click from a bare move/drag and lets applications
    // perform their own multi-click detection.
    if (click_state)
        CGEventSetIntegerValueField(event, kCGMouseEventClickState, click_state);

    postEvent(event);
}

//--------------------------------------------------------------------------------------------------
void postScrollEvent(int32_t lines)
{
    CGEventRef event = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, lines);
    if (!event)
    {
        LOG(ERROR) << "CGEventCreateScrollWheelEvent failed";
        return;
    }

    postEvent(event);
}

// The login window rejects CGEventPost (it is Accessibility-gated, and that grant cannot exist before
// login), so the login-window agent falls back to the deprecated CGPost* family, which still injects
// there - the same fallback other remote-desktop tools use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

//--------------------------------------------------------------------------------------------------
void postLegacyMouse(CGPoint position, quint32 mask)
{
    CGPostMouseEvent(position, true, 3,
                     (mask & proto::input::MouseEvent::LEFT_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::RIGHT_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::MIDDLE_BUTTON) != 0);
}

//--------------------------------------------------------------------------------------------------
void postLegacyKey(int virtual_key, bool down)
{
    CGPostKeyboardEvent(0, static_cast<CGKeyCode>(virtual_key), down);
}

//--------------------------------------------------------------------------------------------------
void postLegacyScroll(int32_t lines)
{
    CGPostScrollWheelEvent(1, lines);
}

#pragma clang diagnostic pop

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorMac::InputInjectorMac(QObject* parent)
    : InputInjector(Type::MAC, parent)
{
    LOG(INFO) << "Ctor";

    // At the login window CGEventPost is rejected (it is Accessibility-gated and no grant can exist
    // pre-login), so fall back to the deprecated CGPost* family there. In a normal user session the
    // modern path works and is kept.
    use_legacy_ = LoginUtils::isActive();
    if (use_legacy_)
        LOG(INFO) << "Using legacy (CGPost*) event injection for the login window";
}

//--------------------------------------------------------------------------------------------------
InputInjectorMac::~InputInjectorMac()
{
    LOG(INFO) << "Dtor";

    // Release any keys still held so they do not get stuck after the session ends.
    for (const quint32& key : std::as_const(pressed_keys_))
    {
        const int keycode = KeycodeConverter::usbKeycodeToNativeKeycode(key);
        if (keycode == KeycodeConverter::invalidNativeKeycode())
            continue;

        postEvent(CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(keycode), false));
    }
}

//--------------------------------------------------------------------------------------------------
// static
InputInjectorMac* InputInjectorMac::create(QObject* parent)
{
    return new InputInjectorMac(parent);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::setScreenInfo(const QSize& /* screen_size */, const QPoint& offset)
{
    // macOS event coordinates are absolute global-display points, so only the offset of the captured
    // region relative to the main display is needed.
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::setBlockInput(bool /* enable */)
{
    // Blocking local input on macOS requires an event tap that swallows physical events (and
    // Accessibility permission). Not supported; remote input is injected alongside local input.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectKeyEvent(const proto::input::KeyEvent& event)
{
    const bool pressed = (event.flags() & proto::input::KeyEvent::PRESSED) != 0;

    if (pressed)
    {
        pressed_keys_.insert(event.usb_keycode());
    }
    else
    {
        if (!pressed_keys_.contains(event.usb_keycode()))
        {
            LOG(INFO) << "No pressed key in the list";
            return;
        }

        pressed_keys_.remove(event.usb_keycode());
    }

    const int keycode = KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (keycode == KeycodeConverter::invalidNativeKeycode())
    {
        LOG(ERROR) << "Invalid key code:" << event.usb_keycode();
        return;
    }

    if (use_legacy_)
    {
        // Modifiers arrive as their own key events, so posting each key independently keeps the state
        // consistent without a separate flags call.
        postLegacyKey(keycode, pressed);
        return;
    }

    CGEventRef key_event =
        CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(keycode), pressed);
    if (!key_event)
    {
        LOG(ERROR) << "CGEventCreateKeyboardEvent failed";
        return;
    }

    // Apply the currently-held modifiers so applications interpret the keystroke correctly (e.g. an
    // uppercase letter while Shift is down). pressed_keys_ already reflects this event.
    const bool caps_lock = (event.flags() & proto::input::KeyEvent::CAPSLOCK) != 0;
    CGEventSetFlags(key_event, static_cast<CGEventFlags>(currentModifierFlags(caps_lock)));

    postEvent(key_event);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTextEvent(const proto::input::TextEvent& event)
{
    const QString text = QString::fromStdString(event.text());
    if (text.isEmpty())
        return;

    // Inject the literal characters independently of the active keyboard layout by attaching the
    // UTF-16 string to the event. The virtual key code is irrelevant in this mode.
    const auto* chars = reinterpret_cast<const UniChar*>(text.utf16());
    const auto length = static_cast<UniCharCount>(text.size());

    CGEventRef key_down = CGEventCreateKeyboardEvent(nullptr, 0, true);
    if (key_down)
    {
        CGEventKeyboardSetUnicodeString(key_down, length, chars);
        postEvent(key_down);
    }

    CGEventRef key_up = CGEventCreateKeyboardEvent(nullptr, 0, false);
    if (key_up)
    {
        CGEventKeyboardSetUnicodeString(key_up, length, chars);
        postEvent(key_up);
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectMouseEvent(const proto::input::MouseEvent& event)
{
    const quint32 mask = event.mask();
    const QPoint pos(event.x() + screen_offset_.x(), event.y() + screen_offset_.y());
    const CGPoint cg_pos = CGPointMake(pos.x(), pos.y());

    if (use_legacy_)
    {
        // CGPostMouseEvent carries the position and button states together; the system derives moves,
        // drags and clicks from the state changes.
        if (pos != last_mouse_pos_ || mask != last_mouse_mask_)
        {
            postLegacyMouse(cg_pos, mask);
            last_mouse_pos_ = pos;
        }

        if (mask & proto::input::MouseEvent::WHEEL_UP)
            postLegacyScroll(1);
        else if (mask & proto::input::MouseEvent::WHEEL_DOWN)
            postLegacyScroll(-1);

        last_mouse_mask_ = mask;
        return;
    }

    // A move must be classified as a drag while a button is held, otherwise applications ignore it
    // during selection.
    if (pos != last_mouse_pos_)
    {
        CGEventType move_type = kCGEventMouseMoved;
        CGMouseButton drag_button = kCGMouseButtonLeft;

        if (mask & proto::input::MouseEvent::LEFT_BUTTON)
        {
            move_type = kCGEventLeftMouseDragged;
            drag_button = kCGMouseButtonLeft;
        }
        else if (mask & proto::input::MouseEvent::RIGHT_BUTTON)
        {
            move_type = kCGEventRightMouseDragged;
            drag_button = kCGMouseButtonRight;
        }
        else if (mask & proto::input::MouseEvent::MIDDLE_BUTTON)
        {
            move_type = kCGEventOtherMouseDragged;
            drag_button = kCGMouseButtonCenter;
        }

        postMouseEvent(move_type, cg_pos, drag_button, 0);
        last_mouse_pos_ = pos;
    }

    // Post an event for every button whose state changed since the previous mouse event.
    const struct
    {
        int flag;
        CGEventType down;
        CGEventType up;
        CGMouseButton button;
    } buttons[] = {
        { proto::input::MouseEvent::LEFT_BUTTON,
          kCGEventLeftMouseDown, kCGEventLeftMouseUp, kCGMouseButtonLeft },
        { proto::input::MouseEvent::RIGHT_BUTTON,
          kCGEventRightMouseDown, kCGEventRightMouseUp, kCGMouseButtonRight },
        { proto::input::MouseEvent::MIDDLE_BUTTON,
          kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGMouseButtonCenter },
        // Back and forward map to the extra buttons 3 and 4.
        { proto::input::MouseEvent::BACK_BUTTON,
          kCGEventOtherMouseDown, kCGEventOtherMouseUp, static_cast<CGMouseButton>(3) },
        { proto::input::MouseEvent::FORWARD_BUTTON,
          kCGEventOtherMouseDown, kCGEventOtherMouseUp, static_cast<CGMouseButton>(4) },
    };

    for (const auto& button : buttons)
    {
        const bool was_down = (last_mouse_mask_ & button.flag) != 0;
        const bool is_down = (mask & button.flag) != 0;

        if (was_down != is_down)
            postMouseEvent(is_down ? button.down : button.up, cg_pos, button.button, 1);
    }

    if (mask & proto::input::MouseEvent::WHEEL_UP)
        postScrollEvent(1);
    else if (mask & proto::input::MouseEvent::WHEEL_DOWN)
        postScrollEvent(-1);

    last_mouse_mask_ = mask;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    // macOS has no public API for synthesizing touch input.
}

//--------------------------------------------------------------------------------------------------
quint64 InputInjectorMac::currentModifierFlags(bool caps_lock) const
{
    CGEventFlags flags = 0;

    for (const quint32& key : pressed_keys_)
    {
        switch (key)
        {
            case kUsbCodeLeftShift:
            case kUsbCodeRightShift:
                flags |= kCGEventFlagMaskShift;
                break;

            case kUsbCodeLeftCtrl:
            case kUsbCodeRightCtrl:
                flags |= kCGEventFlagMaskControl;
                break;

            case kUsbCodeLeftAlt:
            case kUsbCodeRightAlt:
                flags |= kCGEventFlagMaskAlternate;
                break;

            case kUsbCodeLeftGui:
            case kUsbCodeRightGui:
                flags |= kCGEventFlagMaskCommand;
                break;

            default:
                break;
        }
    }

    if (caps_lock)
        flags |= kCGEventFlagMaskAlphaShift;

    return flags;
}
