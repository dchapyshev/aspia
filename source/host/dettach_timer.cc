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

#include "host/dettach_timer.h"

#include "base/logging.h"

#include <QTimerEvent>

namespace host {

namespace {

const std::chrono::seconds kTimeout{ 60 };

} // namespace

DettachTimer::DettachTimer(QObject* parent)
    : QObject(parent)
{
    // Nothing
}

DettachTimer::~DettachTimer() = default;

bool DettachTimer::hasTimer(base::win::SessionId session_id)
{
    return dettach_timers_.find(session_id) != dettach_timers_.end();
}

bool DettachTimer::sessionConnect(base::win::SessionId session_id)
{
    auto result = dettach_timers_.find(session_id);
    if (result != dettach_timers_.end())
    {
        killTimer(result->second);
        dettach_timers_.erase(result);

        return true;
    }

    return false;
}

void DettachTimer::sessionDisconnect(base::win::SessionId session_id)
{
    if (!hasTimer(session_id))
    {
        int timer_id = startTimer(kTimeout);
        CHECK_NE(timer_id, 0);
        dettach_timers_.emplace(session_id, timer_id);
    }
}

void DettachTimer::sessionLogoff(base::win::SessionId session_id)
{
    for (auto it = dettach_timers_.begin(); it != dettach_timers_.end();)
    {
        if (it->first == session_id)
        {
            killTimer(it->second);
            it = dettach_timers_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void DettachTimer::timerEvent(QTimerEvent* event)
{
    for (auto it = dettach_timers_.begin(); it != dettach_timers_.end();)
    {
        if (it->second == event->timerId())
        {
            killTimer(event->timerId());

            emit dettachSession(it->first);

            it = dettach_timers_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    QObject::timerEvent(event);
}

} // namespace host
