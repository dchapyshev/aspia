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

#ifndef CLIENT_ANDROID_KEY_BAR_H
#define CLIENT_ANDROID_KEY_BAR_H

#include <QList>
#include <QWidget>

class Button;
class QHBoxLayout;

// Row of keys shown above the on-screen keyboard for keys it does not provide: sticky modifiers
// (Ctrl/Alt/Shift/Win), navigation/special keys and a Ctrl+Alt+Del shortcut. It only reports the
// taps; the key sequences are composed by DesktopView.
class KeyBar final : public QWidget
{
    Q_OBJECT

public:
    explicit KeyBar(QWidget* parent = nullptr);
    ~KeyBar() final;

public slots:
    // Clears the pressed state of every modifier button (the sticky modifiers were consumed).
    void clearModifiers();

signals:
    void sig_modifierToggled(quint32 usb_keycode, bool active);
    void sig_specialKey(quint32 usb_keycode);

private:
    void addModifier(QHBoxLayout* layout, const QString& text, quint32 usb_keycode);
    void addSpecialKey(QHBoxLayout* layout, const QString& text, quint32 usb_keycode);

    QList<Button*> modifier_buttons_;

    Q_DISABLE_COPY_MOVE(KeyBar)
};

#endif // CLIENT_ANDROID_KEY_BAR_H
