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
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"

namespace {

// Injection uses the deprecated CGPost* family rather than the modern CGEventPost. The login window
// enables Secure Event Input to protect the password field, which suppresses events posted through
// CGEventPost; the CGPost* family injects at a lower level that is not filtered and therefore works
// both at the login window and in a normal user session.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

//--------------------------------------------------------------------------------------------------
void postMouseEvent(CGPoint position, quint32 mask)
{
    // CGPostMouseEvent button order: left, right, middle, then the extra buttons (back, forward).
    CGPostMouseEvent(position, true, 5,
                     (mask & proto::input::MouseEvent::LEFT_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::RIGHT_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::MIDDLE_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::BACK_BUTTON) != 0,
                     (mask & proto::input::MouseEvent::FORWARD_BUTTON) != 0);
}

//--------------------------------------------------------------------------------------------------
void postKeyEvent(int virtual_key, bool down)
{
    CGPostKeyboardEvent(0, static_cast<CGKeyCode>(virtual_key), down);
}

//--------------------------------------------------------------------------------------------------
void postCharEvent(CGCharCode character)
{
    CGPostKeyboardEvent(character, 0, true);
    CGPostKeyboardEvent(character, 0, false);
}

//--------------------------------------------------------------------------------------------------
void postScrollEvent(int32_t lines)
{
    CGPostScrollWheelEvent(1, lines);
}

//--------------------------------------------------------------------------------------------------
void postHScrollEvent(int32_t lines)
{
    // The second wheel axis is horizontal; a positive value scrolls left.
    CGPostScrollWheelEvent(2, 0, lines);
}

#pragma clang diagnostic pop

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorMac::InputInjectorMac(QObject* parent)
    : InputInjector(Type::MAC, parent)
{
    LOG(INFO) << "Ctor";
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

        postKeyEvent(keycode, false);
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

    // Modifiers arrive as their own key events, so posting each key independently keeps the state
    // consistent without a separate flags call.
    postKeyEvent(keycode, pressed);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTextEvent(const proto::input::TextEvent& event)
{
    const QString text = QString::fromStdString(event.text());

    // CGPostKeyboardEvent carries the character in its char-code argument, independent of the active
    // keyboard layout.
    for (int i = 0; i < text.size(); ++i)
        postCharEvent(static_cast<CGCharCode>(text.at(i).unicode()));
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectMouseEvent(const proto::input::MouseEvent& event)
{
    const quint32 mask = event.mask();
    const QPoint pos(event.x() + screen_offset_.x(), event.y() + screen_offset_.y());
    const CGPoint cg_pos = CGPointMake(pos.x(), pos.y());

    // CGPostMouseEvent carries the position and button states together; the system derives moves,
    // drags and clicks from the state changes.
    if (pos != last_mouse_pos_ || mask != last_mouse_mask_)
    {
        postMouseEvent(cg_pos, mask);
        last_mouse_pos_ = pos;
    }

    if (mask & proto::input::MouseEvent::WHEEL_UP)
        postScrollEvent(1);
    else if (mask & proto::input::MouseEvent::WHEEL_DOWN)
        postScrollEvent(-1);

    if (mask & proto::input::MouseEvent::WHEEL_LEFT)
        postHScrollEvent(1);
    else if (mask & proto::input::MouseEvent::WHEEL_RIGHT)
        postHScrollEvent(-1);

    last_mouse_mask_ = mask;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    // macOS has no public API for synthesizing touch input.
}
