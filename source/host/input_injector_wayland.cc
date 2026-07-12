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

#include "host/input_injector_wayland.h"

#include <climits>

#include "base/logging.h"
#include "common/keycode_converter.h"
#include "host/linux/wayland_compositor_source.h"
#include "proto/desktop_input.h"

namespace {

// Linux evdev button codes (from <linux/input-event-codes.h>).
const qint32 kBtnLeft = 0x110;
const qint32 kBtnRight = 0x111;
const qint32 kBtnMiddle = 0x112;
const qint32 kBtnSide = 0x113;
const qint32 kBtnExtra = 0x114;

// X11 keycodes are offset by 8 from the kernel/evdev keycodes the portal expects.
const int kXkbKeycodeOffset = 8;

// Discrete wheel axes used by NotifyPointerAxisDiscrete.
const quint32 kAxisVertical = 0;
const quint32 kAxisHorizontal = 1;

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorWayland::InputInjectorWayland(WaylandCompositorSource* source, QObject* parent)
    : InputInjector(Type::WAYLAND, parent),
      source_(source)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
InputInjectorWayland::~InputInjectorWayland() = default;

//--------------------------------------------------------------------------------------------------
// static
InputInjectorWayland* InputInjectorWayland::create(WaylandCompositorSource* source, QObject* parent)
{
    return new InputInjectorWayland(source, parent);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::setScreenInfo(const QSize& screen_size, const QPoint& offset)
{
    screen_size_ = screen_size;
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::setBlockInput(bool /* enable */)
{
    // Not supported on Wayland: the portal cannot block local input.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::injectKeyEvent(const proto::input::KeyEvent& event)
{
    if (!source_)
        return;

    int keycode = KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (keycode == KeycodeConverter::invalidNativeKeycode())
    {
        LOG(ERROR) << "Invalid key code:" << event.usb_keycode();
        return;
    }

    bool pressed = (event.flags() & proto::input::KeyEvent::PRESSED) != 0;
    source_->notifyKeyboardKeycode(keycode - kXkbKeycodeOffset, pressed);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::injectTextEvent(const proto::input::TextEvent& event)
{
    if (!source_)
        return;

    QString text = QString::fromStdString(event.text());
    if (text.isEmpty())
        return;

    // toUcs4 decodes surrogate pairs, so characters outside the BMP are injected correctly.
    const QList<uint> code_points = text.toUcs4();
    for (uint code_point : code_points)
        injectUnicode(code_point);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::injectMouseEvent(const proto::input::MouseEvent& event)
{
    if (!source_)
        return;

    // The client coordinates are untrusted; add the offset in 64-bit so the sum cannot overflow a
    // signed int, and clamp the result into the captured screen.
    const qint64 max_x = (screen_size_.width() > 0) ? screen_size_.width() - 1 : INT_MAX;
    const qint64 max_y = (screen_size_.height() > 0) ? screen_size_.height() - 1 : INT_MAX;

    QPointF pos(qBound<qint64>(0, static_cast<qint64>(event.x()) + screen_offset_.x(), max_x),
                qBound<qint64>(0, static_cast<qint64>(event.y()) + screen_offset_.y(), max_y));

    // Client coordinates are in the captured frame's pixel space - the monitor's physical resolution.
    // The portal expects pointer coordinates in the stream's logical coordinate space, so on a scaled
    // output (e.g. KDE fractional scaling) the two differ and the coordinates must be rescaled.
    const QSize logical_size = source_->stream().size;
    if (logical_size.isValid() && screen_size_.width() > 0 && screen_size_.height() > 0)
    {
        pos.setX(pos.x() * logical_size.width() / screen_size_.width());
        pos.setY(pos.y() * logical_size.height() / screen_size_.height());
    }

    source_->notifyPointerMotionAbsolute(pos.x(), pos.y());

    bool left_button_pressed = (event.mask() & proto::input::MouseEvent::LEFT_BUTTON) != 0;
    bool middle_button_pressed = (event.mask() & proto::input::MouseEvent::MIDDLE_BUTTON) != 0;
    bool right_button_pressed = (event.mask() & proto::input::MouseEvent::RIGHT_BUTTON) != 0;
    bool back_button_pressed = (event.mask() & proto::input::MouseEvent::BACK_BUTTON) != 0;
    bool forward_button_pressed = (event.mask() & proto::input::MouseEvent::FORWARD_BUTTON) != 0;

    if (left_button_pressed_ != left_button_pressed)
    {
        source_->notifyPointerButton(kBtnLeft, left_button_pressed);
        left_button_pressed_ = left_button_pressed;
    }

    if (middle_button_pressed_ != middle_button_pressed)
    {
        source_->notifyPointerButton(kBtnMiddle, middle_button_pressed);
        middle_button_pressed_ = middle_button_pressed;
    }

    if (right_button_pressed_ != right_button_pressed)
    {
        source_->notifyPointerButton(kBtnRight, right_button_pressed);
        right_button_pressed_ = right_button_pressed;
    }

    if (back_button_pressed_ != back_button_pressed)
    {
        source_->notifyPointerButton(kBtnSide, back_button_pressed);
        back_button_pressed_ = back_button_pressed;
    }

    if (forward_button_pressed_ != forward_button_pressed)
    {
        source_->notifyPointerButton(kBtnExtra, forward_button_pressed);
        forward_button_pressed_ = forward_button_pressed;
    }

    if (event.mask() & proto::input::MouseEvent::WHEEL_UP)
        source_->notifyPointerAxisDiscrete(kAxisVertical, -1);
    else if (event.mask() & proto::input::MouseEvent::WHEEL_DOWN)
        source_->notifyPointerAxisDiscrete(kAxisVertical, 1);

    // A positive horizontal step scrolls right.
    if (event.mask() & proto::input::MouseEvent::WHEEL_RIGHT)
        source_->notifyPointerAxisDiscrete(kAxisHorizontal, 1);
    else if (event.mask() & proto::input::MouseEvent::WHEEL_LEFT)
        source_->notifyPointerAxisDiscrete(kAxisHorizontal, -1);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorWayland::injectUnicode(uint code_point)
{
    // The RemoteDesktop portal accepts keysyms directly, so arbitrary Unicode is injected without
    // the keycode-remapping trick required on X11. Latin-1 code points map straight to keysyms;
    // anything else uses the Unicode keysym range (0x01000000 + code point).
    if (code_point < 0x20)
        return;

    qint32 keysym = (code_point <= 0x00ff) ?
        static_cast<qint32>(code_point) : static_cast<qint32>(0x01000000 | code_point);

    source_->notifyKeyboardKeysym(keysym, true);
    source_->notifyKeyboardKeysym(keysym, false);
}
