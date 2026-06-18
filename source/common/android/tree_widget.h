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

#ifndef COMMON_ANDROID_TREE_WIDGET_H
#define COMMON_ANDROID_TREE_WIDGET_H

#include <QPoint>
#include <QTreeWidget>

class QTimer;

// QTreeWidget adapted for touch screens: tall rows with a full-width selection state layer, a
// chevron expand indicator (toggled by tapping the chevron column) and kinetic finger scrolling.
// Keeps the QTreeWidget API, so it is used the same way.
class TreeWidget final : public QTreeWidget
{
    Q_OBJECT

public:
    explicit TreeWidget(QWidget* parent = nullptr);
    ~TreeWidget() final;

signals:
    void sig_itemLongPressed(QTreeWidgetItem* item);

protected:
    // QTreeWidget implementation.
    void changeEvent(QEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option,
                 const QModelIndex& index) const final;
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const final;

private:
    // Paints the list on the window background instead of the lighter base color.
    void applyBackgroundColor();

    bool applying_palette_ = false;
    QTimer* long_press_timer_ = nullptr;
    QPoint press_pos_;
    bool long_pressed_ = false;

    Q_DISABLE_COPY_MOVE(TreeWidget)
};

#endif // COMMON_ANDROID_TREE_WIDGET_H
