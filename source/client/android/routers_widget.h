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

#ifndef CLIENT_ANDROID_ROUTERS_WIDGET_H
#define CLIENT_ANDROID_ROUTERS_WIDGET_H

#include <QList>
#include <QWidget>

class IconButton;
class TreeWidget;

// Routers screen for the Android client: the list of configured routers. The add and edit actions
// are exposed via appBarActions() to be hosted in the top app bar.
class RoutersWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RoutersWidget(QWidget* parent = nullptr);
    ~RoutersWidget() final;

    QList<QWidget*> appBarActions() const;

    void reload();

private:
    TreeWidget* list_;
    IconButton* add_button_;
    IconButton* edit_button_;

    Q_DISABLE_COPY_MOVE(RoutersWidget)
};

#endif // CLIENT_ANDROID_ROUTERS_WIDGET_H
