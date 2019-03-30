//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__DETTACH_TIMER_H
#define HOST__DETTACH_TIMER_H

#include "base/macros_magic.h"
#include "base/win/session_id.h"

#include <QObject>

namespace host {

class DettachTimer : public QObject
{
    Q_OBJECT

public:
    explicit DettachTimer(QObject* parent = nullptr);
    ~DettachTimer();

    bool hasTimer(base::win::SessionId session_id);

    bool sessionConnect(base::win::SessionId session_id);
    void sessionDisconnect(base::win::SessionId session_id);
    void sessionLogoff(base::win::SessionId session_id);

signals:
    void dettachSession(base::win::SessionId session_id);

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) override;

private:
    std::map<base::win::SessionId, int> dettach_timers_;

    DISALLOW_COPY_AND_ASSIGN(DettachTimer);
};

} // namespace host

#endif // HOST__DETTACH_TIMER_H
