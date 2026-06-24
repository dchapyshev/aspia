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

#include "host/input_injector_uinput.h"

#include <linux/input-event-codes.h>
#include <linux/uinput.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>

#include "base/logging.h"
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"

namespace {

// The absolute pointer reports into a fixed logical range; the compositor maps it onto the output, so
// the same device works for any resolution.
const qint32 kAbsMax = 65535;

// X11 keycodes are offset by 8 from the kernel/evdev keycodes uinput expects.
const int kXkbKeycodeOffset = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorUinput::InputInjectorUinput(QObject* parent)
    : InputInjector(parent)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
InputInjectorUinput::~InputInjectorUinput()
{
    if (fd_ >= 0)
    {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }
}

//--------------------------------------------------------------------------------------------------
// static
InputInjectorUinput* InputInjectorUinput::create(QObject* parent)
{
    std::unique_ptr<InputInjectorUinput> self(new InputInjectorUinput(parent));
    if (!self->init())
        return nullptr;
    return self.release();
}

//--------------------------------------------------------------------------------------------------
bool InputInjectorUinput::init()
{
    fd_ = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0)
    {
        PLOG(ERROR) << "Unable to open /dev/uinput";
        return false;
    }

    // Keyboard keys (the evdev range covered by the keycode converter) and pointer buttons.
    ioctl(fd_, UI_SET_EVBIT, EV_KEY);
    for (int key = 0; key < 256; ++key)
        ioctl(fd_, UI_SET_KEYBIT, key);
    for (int button : { BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA })
        ioctl(fd_, UI_SET_KEYBIT, button);

    // Absolute pointer.
    ioctl(fd_, UI_SET_EVBIT, EV_ABS);
    ioctl(fd_, UI_SET_ABSBIT, ABS_X);
    ioctl(fd_, UI_SET_ABSBIT, ABS_Y);

    // Wheel.
    ioctl(fd_, UI_SET_EVBIT, EV_REL);
    ioctl(fd_, UI_SET_RELBIT, REL_WHEEL);

    ioctl(fd_, UI_SET_EVBIT, EV_SYN);

    for (quint16 axis : { quint16(ABS_X), quint16(ABS_Y) })
    {
        struct uinput_abs_setup abs_setup;
        memset(&abs_setup, 0, sizeof(abs_setup));
        abs_setup.code = axis;
        abs_setup.absinfo.minimum = 0;
        abs_setup.absinfo.maximum = kAbsMax;
        if (ioctl(fd_, UI_ABS_SETUP, &abs_setup) < 0)
            PLOG(ERROR) << "UI_ABS_SETUP failed";
    }

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1234;
    setup.id.product = 0x5678;
    strcpy(setup.name, "Aspia Virtual Input");

    if (ioctl(fd_, UI_DEV_SETUP, &setup) < 0)
    {
        PLOG(ERROR) << "UI_DEV_SETUP failed";
        return false;
    }

    if (ioctl(fd_, UI_DEV_CREATE) < 0)
    {
        PLOG(ERROR) << "UI_DEV_CREATE failed";
        return false;
    }

    LOG(INFO) << "uinput virtual input device created";
    return true;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::emitEvent(quint16 type, quint16 code, qint32 value)
{
    if (fd_ < 0)
        return;

    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    if (::write(fd_, &event, sizeof(event)) != static_cast<ssize_t>(sizeof(event)))
        PLOG(ERROR) << "uinput write failed";
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::setScreenInfo(const QSize& screen_size, const QPoint& offset)
{
    screen_size_ = screen_size;
    screen_offset_ = offset;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::setBlockInput(bool /* enable */)
{
    // Not supported.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::injectKeyEvent(const proto::input::KeyEvent& event)
{
    int keycode = KeycodeConverter::usbKeycodeToNativeKeycode(event.usb_keycode());
    if (keycode == KeycodeConverter::invalidNativeKeycode())
    {
        LOG(ERROR) << "Invalid key code:" << event.usb_keycode();
        return;
    }

    const bool pressed = (event.flags() & proto::input::KeyEvent::PRESSED) != 0;
    emitEvent(EV_KEY, static_cast<quint16>(keycode - kXkbKeycodeOffset), pressed ? 1 : 0);
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::injectTextEvent(const proto::input::TextEvent& /* event */)
{
    // uinput delivers key codes, not Unicode; text injection is not supported on this path.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::injectMouseEvent(const proto::input::MouseEvent& event)
{
    if (abs_mapped_)
    {
        // Exact mapping from the compositor's logical layout (handles multi-monitor offset and scale).
        emitEvent(EV_ABS, ABS_X,
                  qBound(0, static_cast<int>(abs_base_x_ + event.x() * abs_scale_x_), int(kAbsMax)));
        emitEvent(EV_ABS, ABS_Y,
                  qBound(0, static_cast<int>(abs_base_y_ + event.y() * abs_scale_y_), int(kAbsMax)));
    }
    else if (screen_size_.width() > 1 && screen_size_.height() > 1)
    {
        const QPoint pos(event.x() + screen_offset_.x(), event.y() + screen_offset_.y());
        emitEvent(EV_ABS, ABS_X, pos.x() * kAbsMax / (screen_size_.width() - 1));
        emitEvent(EV_ABS, ABS_Y, pos.y() * kAbsMax / (screen_size_.height() - 1));
    }

    const bool left = (event.mask() & proto::input::MouseEvent::LEFT_BUTTON) != 0;
    const bool middle = (event.mask() & proto::input::MouseEvent::MIDDLE_BUTTON) != 0;
    const bool right = (event.mask() & proto::input::MouseEvent::RIGHT_BUTTON) != 0;
    const bool back = (event.mask() & proto::input::MouseEvent::BACK_BUTTON) != 0;
    const bool forward = (event.mask() & proto::input::MouseEvent::FORWARD_BUTTON) != 0;

    if (left_button_pressed_ != left)
    {
        emitEvent(EV_KEY, BTN_LEFT, left ? 1 : 0);
        left_button_pressed_ = left;
    }
    if (middle_button_pressed_ != middle)
    {
        emitEvent(EV_KEY, BTN_MIDDLE, middle ? 1 : 0);
        middle_button_pressed_ = middle;
    }
    if (right_button_pressed_ != right)
    {
        emitEvent(EV_KEY, BTN_RIGHT, right ? 1 : 0);
        right_button_pressed_ = right;
    }
    if (back_button_pressed_ != back)
    {
        emitEvent(EV_KEY, BTN_SIDE, back ? 1 : 0);
        back_button_pressed_ = back;
    }
    if (forward_button_pressed_ != forward)
    {
        emitEvent(EV_KEY, BTN_EXTRA, forward ? 1 : 0);
        forward_button_pressed_ = forward;
    }

    if (event.mask() & proto::input::MouseEvent::WHEEL_UP)
        emitEvent(EV_REL, REL_WHEEL, 1);
    else if (event.mask() & proto::input::MouseEvent::WHEEL_DOWN)
        emitEvent(EV_REL, REL_WHEEL, -1);

    emitEvent(EV_SYN, SYN_REPORT, 0);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::setAbsMapping(double scale_x, double base_x, double scale_y, double base_y)
{
    abs_scale_x_ = scale_x;
    abs_base_x_ = base_x;
    abs_scale_y_ = scale_y;
    abs_base_y_ = base_y;
    abs_mapped_ = true;
}
