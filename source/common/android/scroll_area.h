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

#ifndef COMMON_ANDROID_SCROLL_AREA_H
#define COMMON_ANDROID_SCROLL_AREA_H

#include <QScrollArea>

// QScrollArea adapted for touch screens: kinetic finger scrolling, hidden scroll bars and no
// frame. Keeps the QScrollArea API, so it is used the same way.
class ScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    explicit ScrollArea(QWidget* parent = nullptr);
    ~ScrollArea() override;

protected:
    // QWidget implementation.
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    // Shows the top fade only when scrolled down from the top, the bottom fade only when more content
    // remains below.
    void updateFades();

    QWidget* top_fade_ = nullptr;
    QWidget* bottom_fade_ = nullptr;

    Q_DISABLE_COPY_MOVE(ScrollArea)
};

#endif // COMMON_ANDROID_SCROLL_AREA_H
