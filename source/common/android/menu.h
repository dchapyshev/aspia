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

#ifndef COMMON_ANDROID_MENU_H
#define COMMON_ANDROID_MENU_H

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

// Popup menu adapted for touch screen.
class Menu final : public QWidget
{
    Q_OBJECT

public:
    explicit Menu(QWidget* parent = nullptr);
    ~Menu() final;

    // |icon_file_path| is an SVG resource path; the icon is tinted to the current theme color when
    // drawn, so it follows a theme change.
    void addItem(const QString& text, const QString& icon_file_path = QString());

    // Shows the menu below |anchor| (global coordinates), aligned to its near edge per the layout
    // direction: dropping from the anchor's right edge in LTR and from its left edge in RTL.
    void popup(const QRect& anchor);

signals:
    void sig_triggered(int index);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;

private:
    struct Item
    {
        QString icon_file_path;
        QString text;
    };

    QRect surfaceRect() const;
    QSize surfaceSize() const;
    int itemAt(const QPoint& pos) const;
    bool hasIcons() const;

    QList<Item> items_;
    QPoint surface_pos_;
    int active_ = -1;

    Q_DISABLE_COPY_MOVE(Menu)
};

#endif // COMMON_ANDROID_MENU_H
