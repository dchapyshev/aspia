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

#include "host/input_injector_vt.h"

#include <utility>

#include "base/desktop/vt_session.h"
#include "proto/desktop_input.h"

namespace {

// Emits the escape sequence for a key into |buf| and returns its length.
int escapeSequence(char* buf, char final)
{
    buf[0] = 0x1b;
    buf[1] = '[';
    buf[2] = final;
    return 3;
}

} // namespace

//--------------------------------------------------------------------------------------------------
InputInjectorVt::InputInjectorVt(std::shared_ptr<VtSession> session, QObject* parent)
    : InputInjector(parent),
      session_(std::move(session))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::setScreenInfo(const QSize& /* screen_size */, const QPoint& /* offset */)
{
    // Nothing: a terminal has no pointer mapping.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::setBlockInput(bool /* enable */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectKeyEvent(const proto::input::KeyEvent& event)
{
    const quint32 usb = event.usb_keycode();
    const bool pressed = (event.flags() & proto::input::KeyEvent::PRESSED) != 0;
    const bool capslock = (event.flags() & proto::input::KeyEvent::CAPSLOCK) != 0;

    // The USB keycode is a full code: (usage_page << 16) | usage. Only the keyboard page (0x07) produces
    // terminal input.
    if ((usb >> 16) != 0x07)
        return;
    const quint32 usage = usb & 0xffff;

    // Track modifier state; modifiers themselves produce no terminal input.
    if (usage == 0xe0 || usage == 0xe4) { ctrl_ = pressed; return; }
    if (usage == 0xe1 || usage == 0xe5) { shift_ = pressed; return; }
    if (usage == 0xe2 || usage == 0xe3 || usage == 0xe6 || usage == 0xe7)
        return;

    if (!pressed)
        return;

    char buf[8];
    int len = 0;

    if (usage >= 0x04 && usage <= 0x1d) // a - z
    {
        if (ctrl_)
        {
            buf[len++] = static_cast<char>((usage - 0x04) + 1); // Ctrl-A .. Ctrl-Z -> 0x01 .. 0x1a
        }
        else
        {
            const char c = static_cast<char>('a' + (usage - 0x04));
            const bool upper = shift_ ^ capslock;
            buf[len++] = upper ? static_cast<char>(c - ('a' - 'A')) : c;
        }
    }
    else
    {
        switch (usage)
        {
            case 0x1e: buf[len++] = shift_ ? '!' : '1'; break;
            case 0x1f: buf[len++] = shift_ ? '@' : '2'; break;
            case 0x20: buf[len++] = shift_ ? '#' : '3'; break;
            case 0x21: buf[len++] = shift_ ? '$' : '4'; break;
            case 0x22: buf[len++] = shift_ ? '%' : '5'; break;
            case 0x23: buf[len++] = shift_ ? '^' : '6'; break;
            case 0x24: buf[len++] = shift_ ? '&' : '7'; break;
            case 0x25: buf[len++] = shift_ ? '*' : '8'; break;
            case 0x26: buf[len++] = shift_ ? '(' : '9'; break;
            case 0x27: buf[len++] = shift_ ? ')' : '0'; break;
            case 0x2d: buf[len++] = shift_ ? '_' : '-'; break;
            case 0x2e: buf[len++] = shift_ ? '+' : '='; break;
            case 0x2f: buf[len++] = shift_ ? '{' : '['; break;
            case 0x30: buf[len++] = shift_ ? '}' : ']'; break;
            case 0x31: buf[len++] = shift_ ? '|' : '\\'; break;
            case 0x33: buf[len++] = shift_ ? ':' : ';'; break;
            case 0x34: buf[len++] = shift_ ? '"' : '\''; break;
            case 0x35: buf[len++] = shift_ ? '~' : '`'; break;
            case 0x36: buf[len++] = shift_ ? '<' : ','; break;
            case 0x37: buf[len++] = shift_ ? '>' : '.'; break;
            case 0x38: buf[len++] = shift_ ? '?' : '/'; break;
            case 0x2c: buf[len++] = ' '; break;

            case 0x28: buf[len++] = '\r'; break;   // Enter
            case 0x29: buf[len++] = 0x1b; break;   // Escape
            case 0x2a: buf[len++] = 0x7f; break;   // Backspace
            case 0x2b: buf[len++] = '\t'; break;   // Tab

            case 0x52: len = escapeSequence(buf, 'A'); break; // Up
            case 0x51: len = escapeSequence(buf, 'B'); break; // Down
            case 0x4f: len = escapeSequence(buf, 'C'); break; // Right
            case 0x50: len = escapeSequence(buf, 'D'); break; // Left
            case 0x4a: len = escapeSequence(buf, 'H'); break; // Home
            case 0x4d: len = escapeSequence(buf, 'F'); break; // End

            case 0x49: buf[0] = 0x1b; buf[1] = '['; buf[2] = '2'; buf[3] = '~'; len = 4; break; // Insert
            case 0x4c: buf[0] = 0x1b; buf[1] = '['; buf[2] = '3'; buf[3] = '~'; len = 4; break; // Delete
            case 0x4b: buf[0] = 0x1b; buf[1] = '['; buf[2] = '5'; buf[3] = '~'; len = 4; break; // PageUp
            case 0x4e: buf[0] = 0x1b; buf[1] = '['; buf[2] = '6'; buf[3] = '~'; len = 4; break; // PageDown

            default:
                return;
        }
    }

    if (len > 0)
        session_->sendInput(buf, len);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectTextEvent(const proto::input::TextEvent& /* event */)
{
    // Not used yet: input arrives as key events.
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectMouseEvent(const proto::input::MouseEvent& /* event */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    // Nothing
}
