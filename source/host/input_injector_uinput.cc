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

#include <QString>

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

//--------------------------------------------------------------------------------------------------
// Maps a printable ASCII character to an evdev key code plus whether Shift is required, assuming a US
// keyboard layout on the host. Characters outside this set (non-Latin scripts, layout-specific keys)
// cannot be resolved without the host keymap and are reported as unmapped.
bool charToKey(char32_t ch, quint16* code, bool* shift)
{
    static const quint16 letters[26] = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z
    };
    static const quint16 digits[10] = {
        KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9
    };

    if (ch >= U'a' && ch <= U'z')
    {
        *code = letters[ch - U'a'];
        *shift = false;
        return true;
    }

    if (ch >= U'A' && ch <= U'Z')
    {
        *code = letters[ch - U'A'];
        *shift = true;
        return true;
    }

    if (ch >= U'0' && ch <= U'9')
    {
        *code = digits[ch - U'0'];
        *shift = false;
        return true;
    }

    struct Symbol
    {
        char32_t ch;
        quint16 code;
        bool shift;
    };

    static const Symbol symbols[] = {
        { U' ',  KEY_SPACE,       false }, { U'\t', KEY_TAB,        false },
        { U'\n', KEY_ENTER,       false }, { U'\r', KEY_ENTER,      false },
        { U'!',  KEY_1,           true  }, { U'@',  KEY_2,          true  },
        { U'#',  KEY_3,           true  }, { U'$',  KEY_4,          true  },
        { U'%',  KEY_5,           true  }, { U'^',  KEY_6,          true  },
        { U'&',  KEY_7,           true  }, { U'*',  KEY_8,          true  },
        { U'(',  KEY_9,           true  }, { U')',  KEY_0,          true  },
        { U'-',  KEY_MINUS,       false }, { U'_',  KEY_MINUS,      true  },
        { U'=',  KEY_EQUAL,       false }, { U'+',  KEY_EQUAL,      true  },
        { U'[',  KEY_LEFTBRACE,   false }, { U'{',  KEY_LEFTBRACE,  true  },
        { U']',  KEY_RIGHTBRACE,  false }, { U'}',  KEY_RIGHTBRACE, true  },
        { U'\\', KEY_BACKSLASH,   false }, { U'|',  KEY_BACKSLASH,  true  },
        { U';',  KEY_SEMICOLON,   false }, { U':',  KEY_SEMICOLON,  true  },
        { U'\'', KEY_APOSTROPHE,  false }, { U'"',  KEY_APOSTROPHE, true  },
        { U'`',  KEY_GRAVE,       false }, { U'~',  KEY_GRAVE,      true  },
        { U',',  KEY_COMMA,       false }, { U'<',  KEY_COMMA,      true  },
        { U'.',  KEY_DOT,         false }, { U'>',  KEY_DOT,        true  },
        { U'/',  KEY_SLASH,       false }, { U'?',  KEY_SLASH,      true  }
    };

    for (const Symbol& symbol : symbols)
    {
        if (symbol.ch == ch)
        {
            *code = symbol.code;
            *shift = symbol.shift;
            return true;
        }
    }

    return false;
}

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
void InputInjectorUinput::injectTextEvent(const proto::input::TextEvent& event)
{
    if (fd_ < 0)
        return;

    // Committed Unicode text (on-screen keyboard, IME). uinput delivers key codes interpreted by the
    // host layout, so each character is typed as its US-layout key sequence.
    for (char32_t ch : QString::fromStdString(event.text()).toUcs4())
    {
        quint16 code = 0;
        bool shift = false;
        if (!charToKey(ch, &code, &shift))
        {
            LOG(WARNING) << "Unmapped character for text injection:" << static_cast<quint32>(ch);
            continue;
        }

        if (shift)
            emitEvent(EV_KEY, KEY_LEFTSHIFT, 1);

        emitEvent(EV_KEY, code, 1);
        emitEvent(EV_KEY, code, 0);

        if (shift)
            emitEvent(EV_KEY, KEY_LEFTSHIFT, 0);

        emitEvent(EV_SYN, SYN_REPORT, 0);
    }
}

//--------------------------------------------------------------------------------------------------
void InputInjectorUinput::injectMouseEvent(const proto::input::MouseEvent& event)
{
    // The capturer reports the screen size and this output's offset (via setScreenInfo); the compositor
    // maps the absolute range over that area. For KMS the capturer folds in the monitor offset and
    // fractional scale, so a plain affine map is all that is needed here.
    if (screen_size_.width() > 1 && screen_size_.height() > 1)
    {
        // The client coordinates are untrusted; clamp them into the screen and compute in 64-bit so the
        // intermediate multiply by kAbsMax cannot overflow a signed int.
        const qint64 max_x = screen_size_.width() - 1;
        const qint64 max_y = screen_size_.height() - 1;

        const qint64 x = qBound<qint64>(0, static_cast<qint64>(event.x()) + screen_offset_.x(), max_x);
        const qint64 y = qBound<qint64>(0, static_cast<qint64>(event.y()) + screen_offset_.y(), max_y);

        emitEvent(EV_ABS, ABS_X, static_cast<int>(x * kAbsMax / max_x));
        emitEvent(EV_ABS, ABS_Y, static_cast<int>(y * kAbsMax / max_y));
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
