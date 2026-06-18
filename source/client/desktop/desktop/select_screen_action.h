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

#ifndef CLIENT_DESKTOP_DESKTOP_SELECT_SCREEN_ACTION_H
#define CLIENT_DESKTOP_DESKTOP_SELECT_SCREEN_ACTION_H

#include <QAction>

#include "proto/desktop_screen.h"

class SelectScreenAction final : public QAction
{
    Q_OBJECT

public:
    SelectScreenAction(int number, const proto::screen::Screen& screen, bool is_primary, QObject* parent)
        : QAction(parent),
          screen_(screen)
    {
        QString text;

        if (is_primary)
            text = tr("Monitor %1 (primary)").arg(number);
        else
            text = tr("Monitor %1").arg(number);

        setText(text);
        setToolTip(text);

        QString icon;
        if (number >= 1 && number <= 9)
            icon = QString(":/img/monitor-%1.svg").arg(number);
        else
            icon = ":/img/monitor-n.svg";

        setIcon(QIcon(icon));
        setCheckable(true);
    }

    const proto::screen::Screen& screen() const { return screen_; }

private:
    proto::screen::Screen screen_;
    Q_DISABLE_COPY_MOVE(SelectScreenAction)
};

#endif // CLIENT_DESKTOP_DESKTOP_SELECT_SCREEN_ACTION_H
