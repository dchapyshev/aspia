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

#ifndef CLIENT_DESKTOP_TAB_WIDGET_H
#define CLIENT_DESKTOP_TAB_WIDGET_H

#include <QTabWidget>

class TabBar;

// QTabWidget subclass that installs a custom TabBar (with detach gesture support). Exists because
// QTabWidget::setTabBar is protected; promoting QTabWidget to this class in the .ui lets us swap
// the tab bar without subclassing every use.
class TabWidget final : public QTabWidget
{
    Q_OBJECT

public:
    explicit TabWidget(QWidget* parent = nullptr);
    ~TabWidget() final;

    TabBar* tabBar() const;

private:
    Q_DISABLE_COPY_MOVE(TabWidget)
};

#endif // CLIENT_DESKTOP_TAB_WIDGET_H
