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

#ifndef COMMON_ANDROID_KEYBOARD_INSET_H
#define COMMON_ANDROID_KEYBOARD_INSET_H

#include <QObject>

class QScrollArea;
class QWidget;

// Keeps the content of a top-level window above the on-screen keyboard for a field inside |scope|.
// After a key press on the on-screen keyboard Android may drop the keyboard from the safe area
// margins while it is still up, so the keyboard rectangle is used instead. The margin goes on the
// window, where Qt takes the larger of it and the safe area margins instead of adding them.
class KeyboardInset final : public QObject
{
    Q_OBJECT

public:
    KeyboardInset(QWidget* window, QWidget* scope);
    ~KeyboardInset() final;

signals:
    void sig_activeChanged(bool active);

protected:
    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onUpdate();
    void onScrollFocusIntoView();

private:
    QScrollArea* focusedScrollArea() const;

    QWidget* window_;
    QWidget* scope_;
    int inset_ = 0;
    bool active_ = false;

    Q_DISABLE_COPY_MOVE(KeyboardInset)
};

#endif // COMMON_ANDROID_KEYBOARD_INSET_H
