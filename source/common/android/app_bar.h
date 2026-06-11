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

#ifndef COMMON_ANDROID_APP_BAR_H
#define COMMON_ANDROID_APP_BAR_H

#include <QWidget>

// Top app bar for touch screens: a title with an optional leading back button. Sits at the top of
// a screen and drives screen-level navigation.
class AppBar final : public QWidget
{
    Q_OBJECT

public:
    explicit AppBar(QWidget* parent = nullptr);
    ~AppBar() final;

    void setTitle(const QString& title);
    const QString& title() const { return title_; }

    void setBackVisible(bool visible);
    bool isBackVisible() const { return back_visible_; }

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void backClicked();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;

private:
    QString title_;
    bool back_visible_;

    Q_DISABLE_COPY_MOVE(AppBar)
};

#endif // COMMON_ANDROID_APP_BAR_H
