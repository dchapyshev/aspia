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

#ifndef COMMON_ANDROID_BOTTOM_NAVIGATION_BAR_H
#define COMMON_ANDROID_BOTTOM_NAVIGATION_BAR_H

#include <QList>
#include <QString>
#include <QWidget>

// Bottom navigation bar for touch screens: a row of equal-width destinations with an icon and a
// label, the selected one highlighted with an accent pill. Switches the top-level app sections.
class BottomNavigationBar final : public QWidget
{
    Q_OBJECT

public:
    explicit BottomNavigationBar(QWidget* parent = nullptr);
    ~BottomNavigationBar() final;

    int addItem(const QString& text, const QString& icon_file_path = QString());
    void setItemText(int index, const QString& text);

    int currentIndex() const { return current_; }
    void setCurrentIndex(int index);

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void currentChanged(int index);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;

private:
    int indexAt(const QPoint& pos) const;

    struct Item
    {
        QString text;
        QString icon_file_path;
    };

    QList<Item> items_;
    int current_;

    Q_DISABLE_COPY_MOVE(BottomNavigationBar)
};

#endif // COMMON_ANDROID_BOTTOM_NAVIGATION_BAR_H
