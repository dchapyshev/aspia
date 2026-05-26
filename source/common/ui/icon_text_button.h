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

#ifndef COMMON_UI_ICON_TEXT_BUTTON_H
#define COMMON_UI_ICON_TEXT_BUTTON_H

#include <QToolButton>

// QToolButton with text and icon laid out side by side with a consistent gap and horizontal
// padding, and with an option to place the icon to the right of the text. Used for buttons that
// look the same regardless of the platform style's QToolButton paint quirks.
class IconTextButton final : public QToolButton
{
    Q_OBJECT

public:
    explicit IconTextButton(QWidget* parent = nullptr);
    ~IconTextButton() final;

    void setIconOnRight(bool on_right);
    bool isIconOnRight() const { return icon_on_right_; }

    // QToolButton implementation.
    QSize sizeHint() const final;

protected:
    // QToolButton implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    bool icon_on_right_ = false;

    Q_DISABLE_COPY_MOVE(IconTextButton)
};

#endif // COMMON_UI_ICON_TEXT_BUTTON_H
