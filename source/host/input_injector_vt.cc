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

#include <QString>

#include <algorithm>
#include <utility>

#include "base/desktop/vt_monitors.h"
#include "proto/desktop_input.h"

//--------------------------------------------------------------------------------------------------
InputInjectorVt::InputInjectorVt(std::shared_ptr<VtMonitors> monitors, QObject* parent)
    : InputInjector(parent),
      monitors_(std::move(monitors))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::setScreenInfo(const QSize& screen_size, const QPoint& /* offset */)
{
    screen_size_ = screen_size;
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
    switch (usage)
    {
        case 0xe0: case 0xe4: ctrl_ = pressed; return;
        case 0xe1: case 0xe5: shift_ = pressed; return;
        case 0xe2: case 0xe6: alt_ = pressed; return;
        case 0xe3: case 0xe7: return; // GUI / meta
        default: break;
    }

    if (!pressed)
        return;

    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;
    if (!session)
        return;

    // Paste shortcuts: Shift+Insert and Ctrl+Shift+V. Consumed so they are not typed into the terminal.
    if ((usage == 0x49 && shift_) || (usage == 0x19 && shift_ && ctrl_))
    {
        paste();
        return;
    }

    // Non-printable keys: libvterm turns them into the right escape sequences (respecting cursor-key mode).
    switch (usage)
    {
        case 0x28: session->inputKey(VtKey::ENTER, shift_, ctrl_, alt_); return;
        case 0x29: session->inputKey(VtKey::ESCAPE, shift_, ctrl_, alt_); return;
        case 0x2a: session->inputKey(VtKey::BACKSPACE, shift_, ctrl_, alt_); return;
        case 0x2b: session->inputKey(VtKey::TAB, shift_, ctrl_, alt_); return;
        case 0x52: session->inputKey(VtKey::UP, shift_, ctrl_, alt_); return;
        case 0x51: session->inputKey(VtKey::DOWN, shift_, ctrl_, alt_); return;
        case 0x50: session->inputKey(VtKey::LEFT, shift_, ctrl_, alt_); return;
        case 0x4f: session->inputKey(VtKey::RIGHT, shift_, ctrl_, alt_); return;
        case 0x4a: session->inputKey(VtKey::HOME, shift_, ctrl_, alt_); return;
        case 0x4d: session->inputKey(VtKey::END, shift_, ctrl_, alt_); return;
        case 0x49: session->inputKey(VtKey::INSERT, shift_, ctrl_, alt_); return;
        case 0x4c: session->inputKey(VtKey::DELETE, shift_, ctrl_, alt_); return;
        case 0x4b: session->inputKey(VtKey::PAGE_UP, shift_, ctrl_, alt_); return;
        case 0x4e: session->inputKey(VtKey::PAGE_DOWN, shift_, ctrl_, alt_); return;
        default: break;
    }

    if (usage >= 0x3a && usage <= 0x45) // F1 - F12
    {
        session->inputFunctionKey(static_cast<int>(usage) - 0x3a + 1, shift_, ctrl_, alt_);
        return;
    }

    char32_t ch = 0;

    if (usage >= 0x04 && usage <= 0x1d) // a - z
    {
        const char base = static_cast<char>('a' + (usage - 0x04));
        const bool upper = shift_ ^ capslock;
        ch = static_cast<char32_t>(upper ? base - ('a' - 'A') : base);
    }
    else
    {
        char c = 0;
        switch (usage)
        {
            case 0x1e: c = shift_ ? '!' : '1'; break;
            case 0x1f: c = shift_ ? '@' : '2'; break;
            case 0x20: c = shift_ ? '#' : '3'; break;
            case 0x21: c = shift_ ? '$' : '4'; break;
            case 0x22: c = shift_ ? '%' : '5'; break;
            case 0x23: c = shift_ ? '^' : '6'; break;
            case 0x24: c = shift_ ? '&' : '7'; break;
            case 0x25: c = shift_ ? '*' : '8'; break;
            case 0x26: c = shift_ ? '(' : '9'; break;
            case 0x27: c = shift_ ? ')' : '0'; break;
            case 0x2d: c = shift_ ? '_' : '-'; break;
            case 0x2e: c = shift_ ? '+' : '='; break;
            case 0x2f: c = shift_ ? '{' : '['; break;
            case 0x30: c = shift_ ? '}' : ']'; break;
            case 0x31: c = shift_ ? '|' : '\\'; break;
            case 0x33: c = shift_ ? ':' : ';'; break;
            case 0x34: c = shift_ ? '"' : '\''; break;
            case 0x35: c = shift_ ? '~' : '`'; break;
            case 0x36: c = shift_ ? '<' : ','; break;
            case 0x37: c = shift_ ? '>' : '.'; break;
            case 0x38: c = shift_ ? '?' : '/'; break;
            case 0x2c: c = ' '; break;
            default: return;
        }
        ch = static_cast<char32_t>(c);
    }

    session->inputUnichar(ch, ctrl_, alt_);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectTextEvent(const proto::input::TextEvent& event)
{
    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;
    if (!session)
        return;

    // Committed Unicode text (on-screen keyboard, IME, ...): type each code point into the terminal.
    for (char32_t ch : QString::fromStdString(event.text()).toUcs4())
        session->inputUnichar(ch, false, false);
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectMouseEvent(const proto::input::MouseEvent& event)
{
    using Mouse = proto::input::MouseEvent;

    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;
    if (!session)
        return;

    int cols = 0;
    int rows = 0;
    if (!session->consoleSize(&cols, &rows) || screen_size_.width() <= 0 || screen_size_.height() <= 0)
        return;

    const int col = std::clamp(event.x() * cols / screen_size_.width(), 0, cols - 1);
    const int row = std::clamp(event.y() * rows / screen_size_.height(), 0, rows - 1);

    const quint32 mask = event.mask();

    // Middle-click pastes the stored clipboard text (classic terminal behavior).
    if (mask & Mouse::MIDDLE_BUTTON)
    {
        if (!middle_down_)
        {
            middle_down_ = true;
            paste();
        }
        return;
    }
    middle_down_ = false;

    // Text selection. Like graphical terminals: a plain left-drag selects while the application has no mouse
    // reporting (e.g. a shell); once an application grabs the mouse (mc, vim), Shift is required.
    const bool left = (mask & Mouse::LEFT_BUTTON) != 0;
    if (selecting_)
    {
        if (left)
        {
            session->setSelection(selection_start_col_, selection_start_row_, col, row);
            return;
        }

        // Released: copy the covered text (only if the selection spans more than the start cell).
        selecting_ = false;
        const bool moved = (col != selection_start_col_ || row != selection_start_row_);
        const std::string text = moved
            ? session->selectionText(selection_start_col_, selection_start_row_, col, row) : std::string();
        session->clearSelection();

        if (!text.empty())
            emit sig_terminalClipboard(QString::fromStdString(text));
        return;
    }
    if (left && (shift_ || !session->mouseActive()))
    {
        selecting_ = true;
        selection_start_col_ = col;
        selection_start_row_ = row;
        session->setSelection(col, row, col, row);
        return;
    }

    // Position first, so button and wheel reports carry the new cell. libvterm emits nothing unless the
    // application enabled mouse reporting.
    session->inputMouseMove(col, row);

    if (mask & Mouse::WHEEL_UP)
        session->inputMouseButton(4, true);
    if (mask & Mouse::WHEEL_DOWN)
        session->inputMouseButton(5, true);

    // Button press/release transitions (libvterm buttons: 1 = left, 3 = right; middle is paste, not sent).
    const quint32 buttons = mask & (Mouse::LEFT_BUTTON | Mouse::RIGHT_BUTTON);
    const quint32 changed = buttons ^ last_buttons_;

    if (changed & Mouse::LEFT_BUTTON)
        session->inputMouseButton(1, (buttons & Mouse::LEFT_BUTTON) != 0);
    if (changed & Mouse::RIGHT_BUTTON)
        session->inputMouseButton(3, (buttons & Mouse::RIGHT_BUTTON) != 0);

    last_buttons_ = buttons;
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::paste()
{
    VtSession* session = monitors_ ? monitors_->activeSession() : nullptr;
    if (session && !clipboard_.isEmpty())
        session->paste(clipboard_.toStdString());
}

//--------------------------------------------------------------------------------------------------
void InputInjectorVt::injectTouchEvent(const proto::input::TouchEvent& /* event */)
{
    // Nothing
}
