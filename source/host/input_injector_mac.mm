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

#import <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

#include "base/logging.h"
#include "base/mac/login_utils.h"
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"

namespace {

struct MouseButton
{
    quint32 mask;
    CGMouseButton button;
    CGEventType down;
    CGEventType up;
    CGEventType dragged;
};

// In the order a drag picks its button when several are held.
const MouseButton kMouseButtons[] =
{
    { proto::input::MouseEvent::LEFT_BUTTON, kCGMouseButtonLeft,
      kCGEventLeftMouseDown, kCGEventLeftMouseUp, kCGEventLeftMouseDragged },
    { proto::input::MouseEvent::RIGHT_BUTTON, kCGMouseButtonRight,
      kCGEventRightMouseDown, kCGEventRightMouseUp, kCGEventRightMouseDragged },
    { proto::input::MouseEvent::MIDDLE_BUTTON, kCGMouseButtonCenter,
      kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGEventOtherMouseDragged },
    { proto::input::MouseEvent::BACK_BUTTON, static_cast<CGMouseButton>(3),
      kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGEventOtherMouseDragged },
    { proto::input::MouseEvent::FORWARD_BUTTON, static_cast<CGMouseButton>(4),
      kCGEventOtherMouseDown, kCGEventOtherMouseUp, kCGEventOtherMouseDragged }
};

const quint32 kButtonsMask = proto::input::MouseEvent::LEFT_BUTTON |
    proto::input::MouseEvent::RIGHT_BUTTON | proto::input::MouseEvent::MIDDLE_BUTTON |
    proto::input::MouseEvent::BACK_BUTTON | proto::input::MouseEvent::FORWARD_BUTTON;

//--------------------------------------------------------------------------------------------------
bool needsEventNumber()
{
    // macOS 27 links a drag and a release to their press by the event number and drops the button
    // state of events without it.
    static const bool result =
        [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:{ 27, 0, 0 }];
    return result;
}

//--------------------------------------------------------------------------------------------------
void postMouseEvent(CGEventType type, const QPoint& pos, CGMouseButton button, qint64 event_number,
                    qint64 click_state, const QPoint& delta)
{
    CGEventRef event = CGEventCreateMouseEvent(nullptr, type, CGPointMake(pos.x(), pos.y()), button);
    if (!event)
    {
        LOG(ERROR) << "CGEventCreateMouseEvent failed";
        return;
    }

    if (type != kCGEventMouseMoved && needsEventNumber())
        CGEventSetIntegerValueField(event, kCGMouseEventNumber, event_number);

    CGEventSetIntegerValueField(event, kCGMouseEventClickState, click_state);
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, delta.x());
    CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, delta.y());

    // Modifiers are injected as separate key events, so a click takes them from the system state.
    CGEventSetFlags(event, CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState));

    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

// The login window enables Secure Event Input to protect the password field, which suppresses events
// posted through CGEventPost. The deprecated CGPost* family injects at a lower level that is not
// filtered and therefore works both at the login window and in a normal user session. The mouse uses
// it only at the login window: CGPostMouseEvent cannot set the event number that macOS 27 needs for
// drags.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

//--------------------------------------------------------------------------------------------------
void postLegacyMouseEvent(CGPoint position, quint32 mask)
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
    : InputInjector(Type::MAC, parent),
      use_post_mouse_event_(LoginUtils::isActive())
{
    LOG(INFO) << "Ctor, login window:" << use_post_mouse_event_;
}

//--------------------------------------------------------------------------------------------------
InputInjectorMac::~InputInjectorMac()
{
    LOG(INFO) << "Dtor";

    InputInjectorMac::releaseAllInput();
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

    if (!use_post_mouse_event_)
    {
        injectButtons(pos, mask & kButtonsMask);
    }
    else if (pos != last_mouse_pos_ || mask != last_mouse_mask_)
    {
        // CGPostMouseEvent carries the position and button states together; the system derives
        // moves, drags and clicks from the state changes.
        postLegacyMouseEvent(cg_pos, mask);
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

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::releaseAllInput()
{
    for (const quint32& key : std::as_const(pressed_keys_))
    {
        const int keycode = KeycodeConverter::usbKeycodeToNativeKeycode(key);
        if (keycode == KeycodeConverter::invalidNativeKeycode())
            continue;

        postKeyEvent(keycode, false);
    }

    pressed_keys_.clear();

    if (last_mouse_mask_)
    {
        if (use_post_mouse_event_)
            postLegacyMouseEvent(CGPointMake(last_mouse_pos_.x(), last_mouse_pos_.y()), 0);
        else
            injectButtons(last_mouse_pos_, 0);

        last_mouse_mask_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorMac::injectButtons(const QPoint& pos, quint32 buttons)
{
    const quint32 held = last_mouse_mask_ & kButtonsMask;

    if (pos != last_mouse_pos_)
    {
        const MouseButton* drag_button = nullptr;
        for (const MouseButton& button : kMouseButtons)
        {
            if (held & button.mask)
            {
                drag_button = &button;
                break;
            }
        }

        const QPoint delta = last_mouse_pos_ == QPoint(-1, -1) ? QPoint() : pos - last_mouse_pos_;

        if (drag_button)
        {
            postMouseEvent(drag_button->dragged, pos, drag_button->button, event_number_,
                           click_state_, delta);
        }
        else
        {
            postMouseEvent(kCGEventMouseMoved, pos, kCGMouseButtonLeft, 0, click_state_, delta);
        }

        last_mouse_pos_ = pos;
    }

    for (const MouseButton& button : kMouseButtons)
    {
        const bool pressed = (buttons & button.mask) != 0;
        if (pressed == ((held & button.mask) != 0))
            continue;

        if (pressed)
        {
            // The same button pressed again quickly at the same place continues the click series.
            const qint64 interval = static_cast<qint64>([NSEvent doubleClickInterval] * 1000);
            const QPoint distance = pos - last_click_pos_;
            const bool same_series = button.mask == last_click_button_ &&
                last_click_timer_.isValid() && last_click_timer_.elapsed() <= interval &&
                qAbs(distance.x()) <= 1 && qAbs(distance.y()) <= 1;

            click_state_ = same_series ? click_state_ + 1 : 1;
            if (click_state_ == 1)
                last_click_pos_ = pos;

            last_click_button_ = button.mask;
            last_click_timer_.start();

            if (event_number_ == 0)
            {
                // The first number has to be above the ones the system has used (as in Deskflow).
                const CGEventSourceStateID state = kCGEventSourceStateHIDSystemState;
                event_number_ = CGEventSourceCounterForEventType(state, kCGEventLeftMouseDown) +
                    CGEventSourceCounterForEventType(state, kCGEventRightMouseDown) +
                    CGEventSourceCounterForEventType(state, kCGEventOtherMouseDown);
            }

            ++event_number_;
        }

        postMouseEvent(pressed ? button.down : button.up, pos, button.button, event_number_,
                       click_state_, QPoint());
    }
}
