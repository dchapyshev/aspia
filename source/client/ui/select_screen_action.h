//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_SELECT_SCREEN_ACTION_H
#define CLIENT_UI_SELECT_SCREEN_ACTION_H

#include "base/macros_magic.h"
#include "proto/desktop_extensions.pb.h"

#include <QAction>

namespace client {

class SelectScreenAction : public QAction
{
    Q_OBJECT

public:
    explicit SelectScreenAction(QObject* parent)
        : QAction(parent)
    {
        setText(tr("Full Desktop"));
        setCheckable(true);
        screen_.set_id(-1);
    }

    SelectScreenAction(const proto::Screen& screen, const QString& title, QObject* parent)
        : QAction(parent),
          screen_(screen)
    {
        setText(title);
        setCheckable(true);
    }

    const proto::Screen& screen() const { return screen_; }

private:
    proto::Screen screen_;

    DISALLOW_COPY_AND_ASSIGN(SelectScreenAction);
};

} // namespace client

#endif // CLIENT_UI_SELECT_SCREEN_ACTION_H
