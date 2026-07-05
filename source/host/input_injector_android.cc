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

#include "host/input_injector_android.h"

#include <QJniObject>
#include <QLineF>
#include <QString>

#include "base/logging.h"
#include "proto/desktop_input.h"

namespace {

const char kInputServiceClass[] = "org/aspia/host/InputService";

// A release whose pointer stayed within this many pixels of the press is treated as a tap; a larger
// movement becomes a swipe (drag).
const qreal kTapMovementThreshold = 16.0;

// Gesture durations, in milliseconds.
const int kTapDurationMs = 1;
const int kLongPressDurationMs = 600;
const int kSwipeDurationMs = 120;
const int kScrollDurationMs = 120;

// Vertical travel of a single wheel-notch scroll gesture, in pixels.
const float kScrollDistance = 300.0f;

// Global accessibility actions (AccessibilityService.GLOBAL_ACTION_*).
const int kGlobalActionBack = 1;
const int kGlobalActionHome = 2;
const int kGlobalActionRecents = 3;

//--------------------------------------------------------------------------------------------------
bool serviceRunning()
{
    return QJniObject::callStaticMethod<jboolean>(kInputServiceClass, "isRunning", "()Z");
}

//--------------------------------------------------------------------------------------------------
void tap(const QPointF& pos, int duration_ms)
{
    QJniObject::callStaticMethod<void>(kInputServiceClass, "tap", "(FFI)V",
        static_cast<jfloat>(pos.x()), static_cast<jfloat>(pos.y()), static_cast<jint>(duration_ms));
}

//--------------------------------------------------------------------------------------------------
void swipe(const QPointF& from, const QPointF& to, int duration_ms)
{
    QJniObject::callStaticMethod<void>(kInputServiceClass, "swipe", "(FFFFI)V",
        static_cast<jfloat>(from.x()), static_cast<jfloat>(from.y()),
        static_cast<jfloat>(to.x()), static_cast<jfloat>(to.y()), static_cast<jint>(duration_ms));
}

//--------------------------------------------------------------------------------------------------
void globalAction(int action)
{
    QJniObject::callStaticMethod<void>(kInputServiceClass, "globalAction", "(I)V",
        static_cast<jint>(action));
}

//--------------------------------------------------------------------------------------------------
void commitText(const QString& text)
{
    QJniObject jni_text = QJniObject::fromString(text);
    QJniObject::callStaticMethod<void>(kInputServiceClass, "commitText", "(Ljava/lang/String;)V",
        jni_text.object<jstring>());
}

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorAndroid::InputInjectorAndroid(QObject* parent)
    : InputInjector(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
InputInjectorAndroid::~InputInjectorAndroid()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
InputInjectorAndroid* InputInjectorAndroid::create(QObject* parent)
{
    // The accessibility service is enabled by the user at runtime, so the injector is always created;
    // events are dropped while the service is off.
    return new InputInjectorAndroid(parent);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::setScreenInfo(const QSize& screen_size, const QPoint& offset)
{
    screen_size_ = screen_size;
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::setBlockInput(bool /* enable */)
{
    // Not supported.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::injectKeyEvent(const proto::input::KeyEvent& event)
{
    // Only key presses are acted on; the accessibility API exposes global navigation actions, not
    // arbitrary key injection.
    if ((event.flags() & proto::input::KeyEvent::PRESSED) == 0)
        return;

    if (!serviceRunning())
        return;

    switch (event.usb_keycode())
    {
        case 0x070029: // Escape.
            globalAction(kGlobalActionBack);
            break;

        case 0x07004A: // Home.
            globalAction(kGlobalActionHome);
            break;

        case 0x070076: // Menu (application).
            globalAction(kGlobalActionRecents);
            break;

        default:
            LOG(INFO) << "Unmapped key for accessibility injection:" << event.usb_keycode();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::injectTextEvent(const proto::input::TextEvent& event)
{
    if (event.text().empty())
        return;

    if (!serviceRunning())
        return;

    // Committed Unicode text (on-screen keyboard, IME) is inserted into the focused editable field.
    commitText(QString::fromStdString(event.text()));
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::injectMouseEvent(const proto::input::MouseEvent& event)
{
    if (!serviceRunning())
        return;

    const QPointF pos(event.x() + screen_offset_.x(), event.y() + screen_offset_.y());
    const quint32 mask = event.mask();

    // Wheel notches map to short scroll swipes centered on the pointer. A wheel-up (content moves up)
    // is a swipe from bottom to top.
    if (mask & proto::input::MouseEvent::WHEEL_UP)
    {
        swipe(pos, QPointF(pos.x(), pos.y() - kScrollDistance), kScrollDurationMs);
        return;
    }
    if (mask & proto::input::MouseEvent::WHEEL_DOWN)
    {
        swipe(pos, QPointF(pos.x(), pos.y() + kScrollDistance), kScrollDurationMs);
        return;
    }

    const bool left = (mask & proto::input::MouseEvent::LEFT_BUTTON) != 0;
    const bool right = (mask & proto::input::MouseEvent::RIGHT_BUTTON) != 0;

    if (left && !left_pressed_)
    {
        left_pressed_ = true;
        left_moved_ = false;
        left_down_pos_ = pos;
    }
    else if (!left && left_pressed_)
    {
        left_pressed_ = false;

        if (!left_moved_ && QLineF(left_down_pos_, pos).length() < kTapMovementThreshold)
            tap(pos, kTapDurationMs);
        else
            swipe(left_down_pos_, pos, kSwipeDurationMs);
    }
    else if (left && left_pressed_)
    {
        if (QLineF(left_down_pos_, pos).length() >= kTapMovementThreshold)
            left_moved_ = true;
    }

    // The right button has no cursor equivalent; a press-release maps to a long press, which surfaces
    // the context menu at that point.
    if (right && !right_pressed_)
    {
        right_pressed_ = true;
    }
    else if (!right && right_pressed_)
    {
        right_pressed_ = false;
        tap(pos, kLongPressDurationMs);
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorAndroid::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    // Multi-touch forwarding as accessibility gestures is not implemented yet.
    NOTIMPLEMENTED();
}
