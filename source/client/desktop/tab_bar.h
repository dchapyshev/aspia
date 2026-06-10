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

#ifndef CLIENT_DESKTOP_TAB_BAR_H
#define CLIENT_DESKTOP_TAB_BAR_H

#include <QTabBar>

class QTimer;

class TabBar final : public QTabBar
{
    Q_OBJECT

public:
    explicit TabBar(QWidget* parent = nullptr);
    ~TabBar() final;

    // Marks a tab as the current drop target for an in-flight detached-window drag. Drawing of
    // the highlight pulses while the target is set. Pass -1 to clear.
    void setDropTarget(int index);

signals:
    // Emitted when the user drags a tab vertically out of the bar bounds. The receiver is expected
    // to remove the tab from the QTabWidget and place it into a detached window at global_pos.
    void sig_tabDetachRequested(int index, const QPoint& global_pos);

protected:
    // QWidget implementation.
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;
    void paintEvent(QPaintEvent* event) final;

    // QTabBar implementation.
    void tabInserted(int index) final;

private slots:
    void onPulseTick();

private:
    int pressed_tab_index_ = -1;
    int drop_target_index_ = -1;
    int pulse_phase_ms_ = 0;
    QTimer* pulse_timer_ = nullptr;

    Q_DISABLE_COPY_MOVE(TabBar)
};

#endif // CLIENT_DESKTOP_TAB_BAR_H
