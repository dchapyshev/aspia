//
// SmartCafe Project
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

#include "host/input_injector_x11.h"

#include "base/logging.h"
#include "base/x11/x11_headers.h"
#include "common/keycode_converter.h"

namespace host {

namespace {

// Pixel-to-wheel-ticks conversion ratio used by GTK.
// From third_party/WebKit/Source/web/gtk/WebInputEventFactory.cpp .
const float kWheelTicksPerPixel = 3.0f / 160.0f;

//--------------------------------------------------------------------------------------------------
bool ignoreXServerGrabs(Display* display, bool ignore)
{
    int test_event_base = 0;
    int test_error_base = 0;
    int major = 0;
    int minor = 0;

    if (!XTestQueryExtension(display, &test_event_base, &test_error_base, &major, &minor))
        return false;

    XTestGrabControl(display, ignore);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool isModifierKey(uint32_t usbKeycode)
{
    return usbKeycode == 0x0700e0 || // Left Control
           usbKeycode == 0x0700e1 || // Left Shift
           usbKeycode == 0x0700e2 || // Left Alt
           usbKeycode == 0x0700e3 || // Left Meta
           usbKeycode == 0x0700e4 || // Right Control
           usbKeycode == 0x0700e5 || // Right Shift
           usbKeycode == 0x0700e6 || // Right Alt
           usbKeycode == 0x0700e7;   // Right Meta
}

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorX11::InputInjectorX11(QObject* parent)
    : InputInjector(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
InputInjectorX11::~InputInjectorX11()
{
    LOG(INFO) << "Dtor";

    releasePressedKeys();
    setAutoRepeatEnabled(true);

    if (display_)
        XCloseDisplay(display_);
}

//--------------------------------------------------------------------------------------------------
// static
InputInjectorX11* InputInjectorX11::create(QObject* parent)
{
    std::unique_ptr<InputInjectorX11> instance(new InputInjectorX11(parent));
    if (!instance->init())
        return nullptr;

    return instance.release();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::setScreenOffset(const QPoint& offset)
{
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::setBlockInput(bool /* enable */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::injectKeyEvent(const proto::desktop::KeyEvent& event)
{
    int keycode = common::KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (keycode == common::KeycodeConverter::invalidNativeKeycode())
    {
        LOG(ERROR) << "Invalid key code:" << event.usb_keycode();
        return;
    }

    bool is_pressed = (event.flags() & proto::desktop::KeyEvent::PRESSED) != 0;
    if (is_pressed)
    {
        if (pressed_keys_.contains(keycode))
        {
            // Ignore repeats for modifier keys.
            if (isModifierKey(event.usb_keycode()))
                return;

            // Key is already held down, so lift the key up to ensure this repeated press takes
            // effect.
            XTestFakeKeyEvent(display_, keycode, X11_False, CurrentTime);
        }

        if (!isLockKey(keycode))
        {
            bool capsLock = (event.flags() & proto::desktop::KeyEvent::CAPSLOCK) != 0;
            bool numLock = (event.flags() & proto::desktop::KeyEvent::NUMLOCK) != 0;
            setLockStates(capsLock, numLock);
        }

        // We turn autorepeat off when we initially connect, but in can get re-enabled when, e.g.,
        // the desktop environment reapplies its settings.
        if (isAutoRepeatEnabled())
            setAutoRepeatEnabled(false);

        pressed_keys_.insert(keycode);
    }
    else
    {
        if (!pressed_keys_.contains(keycode))
        {
            LOG(INFO) << "Button release event has arrived, but such a button has "
                         "not been pressed before";
            return;
        }

        pressed_keys_.remove(keycode);
    }

    XTestFakeKeyEvent(display_, keycode, is_pressed, CurrentTime);
    XFlush(display_);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::injectTextEvent(const proto::desktop::TextEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::injectMouseEvent(const proto::desktop::MouseEvent& event)
{
    QPoint pos(event.x(), event.y());
    pos += screen_offset_;

    bool left_button_pressed = (event.mask() & proto::desktop::MouseEvent::LEFT_BUTTON) != 0;
    bool middle_button_pressed = (event.mask() & proto::desktop::MouseEvent::MIDDLE_BUTTON) != 0;
    bool right_button_pressed = (event.mask() & proto::desktop::MouseEvent::RIGHT_BUTTON) != 0;
    bool back_button_pressed = (event.mask() & proto::desktop::MouseEvent::BACK_BUTTON) != 0;
    bool forward_button_pressed = (event.mask() & proto::desktop::MouseEvent::FORWARD_BUTTON) != 0;

    bool inject_motion = true;

    // Injecting a motion event immediately before a button release results in a MotionNotify even
    // if the mouse position hasn't changed, which confuses apps which assume MotionNotify implies
    // movement. See crbug.com/138075.
    if ((left_button_pressed != left_button_pressed_ && !left_button_pressed) ||
        (middle_button_pressed != middle_button_pressed_ && !middle_button_pressed) ||
        (right_button_pressed != right_button_pressed_ && !right_button_pressed) ||
        (back_button_pressed != back_button_pressed_ && !back_button_pressed) ||
        (forward_button_pressed != forward_button_pressed_ && !forward_button_pressed))
    {
        if (last_mouse_pos_ == pos)
            inject_motion = false;
    }

    if (inject_motion)
    {
        last_mouse_pos_.setX(std::max(0, pos.x()));
        last_mouse_pos_.setY(std::max(0, pos.y()));

        XTestFakeMotionEvent(display_, DefaultScreen(display_),
                             last_mouse_pos_.x(), last_mouse_pos_.y(),
                             CurrentTime);
    }

    if (left_button_pressed_ != left_button_pressed)
    {
        XTestFakeButtonEvent(display_, pointer_button_map_[0], left_button_pressed, CurrentTime);
        left_button_pressed_ = left_button_pressed;
    }

    if (middle_button_pressed_ != middle_button_pressed)
    {
        XTestFakeButtonEvent(display_, pointer_button_map_[1], middle_button_pressed, CurrentTime);
        middle_button_pressed_ = middle_button_pressed;
    }

    if (right_button_pressed_ != right_button_pressed)
    {
        XTestFakeButtonEvent(display_, pointer_button_map_[2], right_button_pressed, CurrentTime);
        right_button_pressed_ = right_button_pressed;
    }

    if (back_button_pressed_ != back_button_pressed)
    {
        XTestFakeButtonEvent(display_, pointer_button_map_[7], back_button_pressed, CurrentTime);
        back_button_pressed_ = back_button_pressed;
    }

    if (forward_button_pressed_ != forward_button_pressed)
    {
        XTestFakeButtonEvent(display_, pointer_button_map_[8], forward_button_pressed, CurrentTime);
        forward_button_pressed_ = forward_button_pressed;
    }

    int wheel_movement = 0;

    if (event.mask() & proto::desktop::MouseEvent::WHEEL_UP)
        wheel_movement = 120;
    else if (event.mask() & proto::desktop::MouseEvent::WHEEL_DOWN)
        wheel_movement = -120;

    if (wheel_movement != 0)
    {
        int wheel_ticks = std::abs(wheel_movement) * kWheelTicksPerPixel;
        int wheel_button;

        if (wheel_movement > 0)
            wheel_button = pointer_button_map_[3];
        else
            wheel_button = pointer_button_map_[4];

        for (int i = 0; i < wheel_ticks; ++i)
        {
            // Generate a button-down and a button-up to simulate a wheel click.
            XTestFakeButtonEvent(display_, wheel_button, true, CurrentTime);
            XTestFakeButtonEvent(display_, wheel_button, false, CurrentTime);
        }
    }

    XFlush(display_);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::injectTouchEvent(const proto::desktop::TouchEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
bool InputInjectorX11::init()
{
    display_ = XOpenDisplay(nullptr);
    if (!display_)
    {
        LOG(ERROR) << "XOpenDisplay failed";
        return false;
    }

    root_window_ = XRootWindow(display_, DefaultScreen(display_));
    if (root_window_ == BadValue)
    {
        LOG(ERROR) << "Unable to get the root window";
        return false;
    }

    if (!ignoreXServerGrabs(display_, true))
    {
        LOG(ERROR) << "Server does not support XTest";
        return false;
    }

    initMouseButtonMap();
    setAutoRepeatEnabled(false);
    return true;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::initMouseButtonMap()
{
    // Do not touch global pointer mapping, since this may affect the local user. Instead, try to
    // work around it by reversing the mapping. Note that if a user has a global mapping that
    // completely disables a button (by assigning 0 to it), we won't be able to inject it.
    int num_buttons = XGetPointerMapping(display_, nullptr, 0);
    std::unique_ptr<unsigned char[]> pointer_mapping(new unsigned char[num_buttons]);
    num_buttons = XGetPointerMapping(display_, pointer_mapping.get(), num_buttons);

    for (int i = 0; i < kNumPointerButtons; i++)
        pointer_button_map_[i] = -1;

    for (int i = 0; i < num_buttons; i++)
    {
        // Reverse the mapping.
        if (pointer_mapping[i] > 0 && pointer_mapping[i] <= kNumPointerButtons)
            pointer_button_map_[pointer_mapping[i] - 1] = i + 1;
    }

    for (int i = 0; i < kNumPointerButtons; i++)
    {
        if (pointer_button_map_[i] == -1)
        {
            LOG(ERROR) << "Global pointer mapping does not support button" << i + 1;
        }
    }

    int opcode, event, error;
    if (!XQueryExtension(display_, "XInputExtension", &opcode, &event, &error))
    {
        // If XInput is not available, we're done. But it would be very unusual to
        // have a server that supports XTest but not XInput, so log it as an error.
        LOG(ERROR) << "X Input extension not available:" << error;
        return;
    }

    // Make sure the XTEST XInput pointer device mapping is trivial. It should be
    // safe to reset this mapping, as it won't affect the user's local devices.
    // In fact, the reason why we do this is because an old gnome-settings-daemon
    // may have mistakenly applied left-handed preferences to the XTEST device.
    XID device_id = 0;
    bool device_found = false;
    int num_devices;

    XDeviceInfo* devices = XListInputDevices(display_, &num_devices);

    for (int i = 0; i < num_devices; i++)
    {
        XDeviceInfo* deviceInfo = &devices[i];
        if (deviceInfo->use == IsXExtensionPointer &&
            strcmp(deviceInfo->name, "Virtual core XTEST pointer") == 0)
        {
            device_id = deviceInfo->id;
            device_found = true;
            break;
        }
    }
    XFreeDeviceList(devices);

    if (!device_found)
    {
        LOG(ERROR) << "Cannot find XTest device.";
        return;
    }

    XDevice* device = XOpenDevice(display_, device_id);
    if (!device)
    {
        LOG(ERROR) << "Cannot open XTest device.";
        return;
    }

    int num_device_buttons = XGetDeviceButtonMapping(display_, device, nullptr, 0);
    std::unique_ptr<unsigned char[]> button_mapping(new unsigned char[num_buttons]);

    for (int i = 0; i < num_device_buttons; i++)
        button_mapping[i] = i + 1;

    error = XSetDeviceButtonMapping(display_, device, button_mapping.get(), num_device_buttons);
    if (error != Success)
    {
        LOG(ERROR) << "Failed to set XTest device button mapping:" << error;
    }

    XCloseDevice(display_, device);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::setLockStates(bool caps, bool num)
{
    // The lock bits associated with each lock key.
    unsigned int caps_lock_mask = XkbKeysymToModifiers(display_, XK_Caps_Lock);
    unsigned int num_lock_mask = XkbKeysymToModifiers(display_, XK_Num_Lock);

    // The lock bits we want to update
    unsigned int update_mask = caps_lock_mask | num_lock_mask;

    // The value of those bits
    unsigned int lock_values = 0;

    if (caps)
        lock_values |= caps_lock_mask;

    if (num)
        lock_values |= num_lock_mask;

    XkbLockModifiers(display_, XkbUseCoreKbd, update_mask, lock_values);
}

//--------------------------------------------------------------------------------------------------
bool InputInjectorX11::isLockKey(int keycode)
{
    XkbStateRec state;
    KeySym keysym;

    if (XkbGetState(display_, XkbUseCoreKbd, &state) == Success &&
        XkbLookupKeySym(display_, keycode, XkbStateMods(&state), nullptr, &keysym) == X11_True)
    {
        return keysym == XK_Caps_Lock || keysym == XK_Num_Lock;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool InputInjectorX11::isAutoRepeatEnabled()
{
    XKeyboardState state;
    if (!XGetKeyboardControl(display_, &state))
    {
        LOG(ERROR) << "Failed to get keyboard auto-repeat status, assuming ON";
        return true;
    }

    return state.global_auto_repeat == AutoRepeatModeOn;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::setAutoRepeatEnabled(bool enable)
{
    if (!display_)
        return;

    XKeyboardControl control;
    control.auto_repeat_mode = enable ? AutoRepeatModeOn : AutoRepeatModeOff;

    XChangeKeyboardControl(display_, KBAutoRepeatMode, &control);
    XFlush(display_);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorX11::releasePressedKeys()
{
    if (!pressed_keys_.empty())
    {
        auto it = pressed_keys_.begin();
        while (it != pressed_keys_.end())
        {
            XTestFakeKeyEvent(display_, *it, 0, CurrentTime);
            XFlush(display_);

            it = pressed_keys_.erase(it);
        }
    }

    if (left_button_pressed_ || middle_button_pressed_ || right_button_pressed_)
    {
        if (left_button_pressed_)
        {
            XTestFakeButtonEvent(display_, pointer_button_map_[0], 0, CurrentTime);
            left_button_pressed_ = false;
        }

        if (middle_button_pressed_)
        {
            XTestFakeButtonEvent(display_, pointer_button_map_[1], 0, CurrentTime);
            middle_button_pressed_ = false;
        }

        if (right_button_pressed_)
        {
            XTestFakeButtonEvent(display_, pointer_button_map_[2], 0, CurrentTime);
            right_button_pressed_ = false;
        }
    }
}

} // namespace host
