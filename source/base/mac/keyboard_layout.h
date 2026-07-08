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

#ifndef BASE_MAC_KEYBOARD_LAYOUT_H
#define BASE_MAC_KEYBOARD_LAYOUT_H

#include <QList>
#include <QString>

// Thin wrapper over the macOS Text Input Sources API. Lets a process pick its own keyboard layout,
// which the elevated (root) settings dialog needs: it runs outside the user's input-source context and
// does not follow the system keyboard-layout switching, so it offers an explicit selector instead.
struct KeyboardLayout
{
    QString id;    // input source id, e.g. "com.apple.keylayout.ABC"
    QString name;  // localized name, e.g. "ABC"
};

// The keyboard layouts the user has enabled and that can be selected.
QList<KeyboardLayout> enabledKeyboardLayouts();

// The id of the currently selected keyboard layout, or an empty string on failure.
QString currentKeyboardLayout();

// Makes the layout identified by |id| the current one for the session.
void selectKeyboardLayout(const QString& id);

#endif // BASE_MAC_KEYBOARD_LAYOUT_H
