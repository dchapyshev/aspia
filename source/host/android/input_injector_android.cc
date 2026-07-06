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

#include "host/android/input_injector_android.h"

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

// Android KeyEvent.KEYCODE_* values (stable framework constants). Only the keys the host translates are
// listed; letters and digits are computed from their contiguous ranges.
const int kKeycode0 = 7;
const int kKeycode1 = 8;
const int kKeycodeDpadUp = 19;
const int kKeycodeDpadDown = 20;
const int kKeycodeDpadLeft = 21;
const int kKeycodeDpadRight = 22;
const int kKeycodeA = 29;
const int kKeycodeComma = 55;
const int kKeycodePeriod = 56;
const int kKeycodeTab = 61;
const int kKeycodeSpace = 62;
const int kKeycodeEnter = 66;
const int kKeycodeDel = 67; // Backspace.
const int kKeycodeGrave = 68;
const int kKeycodeMinus = 69;
const int kKeycodeEquals = 70;
const int kKeycodeLeftBracket = 71;
const int kKeycodeRightBracket = 72;
const int kKeycodeBackslash = 73;
const int kKeycodeSemicolon = 74;
const int kKeycodeApostrophe = 75;
const int kKeycodeSlash = 76;
const int kKeycodePageUp = 92;
const int kKeycodePageDown = 93;
const int kKeycodeForwardDel = 112;
const int kKeycodeCapsLock = 115;
const int kKeycodeMoveHome = 122;
const int kKeycodeMoveEnd = 123;
const int kKeycodeInsert = 124;

// Android KeyEvent meta state bits.
const int kMetaShiftOn = 0x00000001;
const int kMetaAltOn = 0x00000002;
const int kMetaCtrlOn = 0x00001000;
const int kMetaMetaOn = 0x00010000;
const int kMetaCapsLockOn = 0x00100000;

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

//--------------------------------------------------------------------------------------------------
void injectKey(int key_code, int meta_state)
{
    QJniObject::callStaticMethod<void>(kInputServiceClass, "injectKey", "(II)V",
        static_cast<jint>(key_code), static_cast<jint>(meta_state));
}

//--------------------------------------------------------------------------------------------------
// Returns the Android meta state bit for a modifier key, or 0 if the key is not a modifier.
int modifierBit(quint32 usb_keycode)
{
    switch (usb_keycode)
    {
        case 0x0700E1: // Left Shift.
        case 0x0700E5: // Right Shift.
            return kMetaShiftOn;

        case 0x0700E0: // Left Ctrl.
        case 0x0700E4: // Right Ctrl.
            return kMetaCtrlOn;

        case 0x0700E2: // Left Alt.
        case 0x0700E6: // Right Alt.
            return kMetaAltOn;

        case 0x0700E3: // Left GUI (Meta).
        case 0x0700E7: // Right GUI (Meta).
            return kMetaMetaOn;

        default:
            return 0;
    }
}

//--------------------------------------------------------------------------------------------------
// Maps a USB HID keycode to an Android KeyEvent.KEYCODE_*. USB HID and Android keycodes are both
// physical-position based; the character the key produces is decided later by the device key character
// map. Returns 0 for keys with no mapping.
int usbToAndroidKeycode(quint32 usb_keycode)
{
    // Letters a-z: USB 0x04..0x1D -> KEYCODE_A..KEYCODE_Z.
    if (usb_keycode >= 0x070004 && usb_keycode <= 0x07001D)
        return kKeycodeA + static_cast<int>(usb_keycode - 0x070004);

    // Digits 1-9: USB 0x1E..0x26 -> KEYCODE_1..KEYCODE_9. The zero key follows separately.
    if (usb_keycode >= 0x07001E && usb_keycode <= 0x070026)
        return kKeycode1 + static_cast<int>(usb_keycode - 0x07001E);

    switch (usb_keycode)
    {
        case 0x070027: return kKeycode0;            // 0.
        case 0x070028: return kKeycodeEnter;        // Return.
        case 0x07002A: return kKeycodeDel;          // Backspace.
        case 0x07002B: return kKeycodeTab;
        case 0x07002C: return kKeycodeSpace;
        case 0x07002D: return kKeycodeMinus;
        case 0x07002E: return kKeycodeEquals;
        case 0x07002F: return kKeycodeLeftBracket;
        case 0x070030: return kKeycodeRightBracket;
        case 0x070031: return kKeycodeBackslash;
        case 0x070033: return kKeycodeSemicolon;
        case 0x070034: return kKeycodeApostrophe;
        case 0x070035: return kKeycodeGrave;
        case 0x070036: return kKeycodeComma;
        case 0x070037: return kKeycodePeriod;
        case 0x070038: return kKeycodeSlash;
        case 0x070039: return kKeycodeCapsLock;
        case 0x070049: return kKeycodeInsert;
        case 0x07004A: return kKeycodeMoveHome;     // Home.
        case 0x07004B: return kKeycodePageUp;
        case 0x07004C: return kKeycodeForwardDel;   // Delete Forward.
        case 0x07004D: return kKeycodeMoveEnd;      // End.
        case 0x07004E: return kKeycodePageDown;
        case 0x07004F: return kKeycodeDpadRight;
        case 0x070050: return kKeycodeDpadLeft;
        case 0x070051: return kKeycodeDpadDown;
        case 0x070052: return kKeycodeDpadUp;
        default:       return 0;
    }
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
    if (!serviceRunning())
        return;

    const quint32 usb_keycode = event.usb_keycode();
    const bool pressed = (event.flags() & proto::input::KeyEvent::PRESSED) != 0;

    // Modifier keys are not injected; they only change the meta state applied to the following keys.
    const int meta_bit = modifierBit(usb_keycode);
    if (meta_bit != 0)
    {
        if (pressed)
            modifiers_ |= meta_bit;
        else
            modifiers_ &= ~meta_bit;
        return;
    }

    // A few keys have no editable equivalent and map to global navigation actions instead.
    switch (usb_keycode)
    {
        case 0x070029: // Escape.
            if (pressed)
                globalAction(kGlobalActionBack);
            return;

        case 0x070076: // Menu (application).
            if (pressed)
                globalAction(kGlobalActionRecents);
            return;

        default:
            break;
    }

    const int key_code = usbToAndroidKeycode(usb_keycode);
    if (key_code == 0)
    {
        if (pressed)
            LOG(INFO) << "Unmapped key for accessibility injection:" << usb_keycode;
        return;
    }

    // The character is decided by Android when the key event is dispatched (US layout), so only the press
    // is forwarded. Caps lock is reported on the event flags rather than as a held modifier.
    if (!pressed)
        return;

    int meta_state = modifiers_;
    if (event.flags() & proto::input::KeyEvent::CAPSLOCK)
        meta_state |= kMetaCapsLockOn;

    injectKey(key_code, meta_state);
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

    // Wheel notches map to short scroll swipes centered on the pointer. On a touchscreen the content
    // follows the finger, so a wheel-up (scroll toward the top) is a swipe downward, and vice versa.
    if (mask & proto::input::MouseEvent::WHEEL_UP)
    {
        swipe(pos, QPointF(pos.x(), pos.y() + kScrollDistance), kScrollDurationMs);
        return;
    }
    if (mask & proto::input::MouseEvent::WHEEL_DOWN)
    {
        swipe(pos, QPointF(pos.x(), pos.y() - kScrollDistance), kScrollDurationMs);
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
