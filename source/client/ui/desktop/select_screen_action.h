//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_DESKTOP_SELECT_SCREEN_ACTION_H
#define CLIENT_UI_DESKTOP_SELECT_SCREEN_ACTION_H

#include "base/macros_magic.h"
#include "proto/desktop_extensions.h"

#include <QAction>

namespace client {

class SelectScreenAction final : public QAction
{
    Q_OBJECT

public:
    explicit SelectScreenAction(QObject* parent)
        : QAction(parent)
    {
        setToolTip(tr("Full Desktop"));
        setIcon(QIcon(":/img/monitors.png"));
        setCheckable(true);
        screen_.set_id(-1);
    }

    SelectScreenAction(int number, const proto::desktop::Screen& screen, bool is_primary, QObject* parent)
        : QAction(parent),
          screen_(screen)
    {
        QString tooltip;

        if (is_primary)
            tooltip = tr("Monitor %1 (primary)").arg(number);
        else
            tooltip = tr("Monitor %1").arg(number);

        setToolTip(tooltip);

        QString icon;
        if (number >= 1 && number <= 9)
            icon = QString(":/img/monitor-%1.png").arg(number);
        else
            icon = ":/img/monitor-n.png";

        setIcon(QIcon(icon));
        setCheckable(true);
    }

    const proto::desktop::Screen& screen() const { return screen_; }

private:
    proto::desktop::Screen screen_;

    DISALLOW_COPY_AND_ASSIGN(SelectScreenAction);
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_SELECT_SCREEN_ACTION_H
