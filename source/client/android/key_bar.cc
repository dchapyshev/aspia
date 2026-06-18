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

#include "client/android/key_bar.h"

#include <QHBoxLayout>
#include <QPalette>

#include <utility>

#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/scroll_area.h"

namespace {

// USB HID usage codes (page 0x07) for the keys offered by the bar.
const quint32 kUsbLeftCtrl  = 0x0700e0;
const quint32 kUsbLeftShift = 0x0700e1;
const quint32 kUsbLeftAlt   = 0x0700e2;
const quint32 kUsbLeftMeta  = 0x0700e3;
const quint32 kUsbEscape    = 0x070029;
const quint32 kUsbTab       = 0x07002b;
const quint32 kUsbDelete    = 0x07004c;
const quint32 kUsbHome      = 0x07004a;
const quint32 kUsbEnd       = 0x07004d;
const quint32 kUsbPageUp    = 0x07004b;
const quint32 kUsbPageDown  = 0x07004e;
const quint32 kUsbRight     = 0x07004f;
const quint32 kUsbLeft      = 0x070050;
const quint32 kUsbDown      = 0x070051;
const quint32 kUsbUp        = 0x070052;

} // namespace

//--------------------------------------------------------------------------------------------------
KeyBar::KeyBar(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Controls::surfaceColor());
    setPalette(pal);

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setWidgetResizable(true);

    QWidget* content = new QWidget(scroll);
    content->setFocusPolicy(Qt::NoFocus);

    QHBoxLayout* row = new QHBoxLayout(content);
    row->setContentsMargins(8, 6, 8, 6);
    row->setSpacing(6);
    row->setSizeConstraint(QLayout::SetMinimumSize);

    addSpecialKey(row, "Esc", kUsbEscape);
    addSpecialKey(row, "Tab", kUsbTab);
    addModifier(row, "Ctrl", kUsbLeftCtrl);
    addModifier(row, "Alt", kUsbLeftAlt);
    addModifier(row, "Shift", kUsbLeftShift);
    addModifier(row, "Win", kUsbLeftMeta);
    addSpecialKey(row, "Left", kUsbLeft);
    addSpecialKey(row, "Up", kUsbUp);
    addSpecialKey(row, "Down", kUsbDown);
    addSpecialKey(row, "Right", kUsbRight);
    addSpecialKey(row, "Home", kUsbHome);
    addSpecialKey(row, "End", kUsbEnd);
    addSpecialKey(row, "PgUp", kUsbPageUp);
    addSpecialKey(row, "PgDn", kUsbPageDown);
    addSpecialKey(row, "Del", kUsbDelete);

    scroll->setWidget(content);

    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->addWidget(scroll);

    setFixedHeight(content->sizeHint().height());
}

//--------------------------------------------------------------------------------------------------
KeyBar::~KeyBar() = default;

//--------------------------------------------------------------------------------------------------
void KeyBar::clearModifiers()
{
    for (Button* button : std::as_const(modifier_buttons_))
        button->setRole(Button::Role::TEXT);
}

//--------------------------------------------------------------------------------------------------
void KeyBar::addModifier(QHBoxLayout* layout, const QString& text, quint32 usb_keycode)
{
    Button* button = new Button(text, Button::Role::TEXT, layout->parentWidget());
    button->setFocusPolicy(Qt::NoFocus);

    connect(button, &QPushButton::clicked, this, [this, button, usb_keycode]()
    {
        // The pressed (filled) state marks a sticky modifier; tapping again cancels it.
        const bool active = (button->role() != Button::Role::FILLED);
        button->setRole(active ? Button::Role::FILLED : Button::Role::TEXT);
        emit sig_modifierToggled(usb_keycode, active);
    });

    layout->addWidget(button);
    modifier_buttons_.append(button);
}

//--------------------------------------------------------------------------------------------------
void KeyBar::addSpecialKey(QHBoxLayout* layout, const QString& text, quint32 usb_keycode)
{
    Button* button = new Button(text, Button::Role::TEXT, layout->parentWidget());
    button->setFocusPolicy(Qt::NoFocus);

    connect(button, &QPushButton::clicked, this, [this, usb_keycode]()
    {
        emit sig_specialKey(usb_keycode);
    });

    layout->addWidget(button);
}
