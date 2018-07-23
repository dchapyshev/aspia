//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/simple_event_dispatcher.h"

#include <QCoreApplication>

namespace aspia {

SimpleEventDispatcher::SimpleEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent)
{
    // Nothing
}

bool SimpleEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    awaken_ = false;
    interrupt_ = false;

    bool result = hasPendingEvents();

    emit awake();

    QCoreApplication::sendPostedEvents();

    bool can_wait = !interrupt_ && (flags & QEventLoop::WaitForMoreEvents) && !result;
    if (can_wait)
        emit aboutToBlock();

    if (!interrupt_)
    {
        if (can_wait)
        {
            event_lock_.lock();
            event_.wait(&event_lock_);

            result |= awaken_;

            event_lock_.unlock();
        }
    }

    return result;
}

bool SimpleEventDispatcher::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount();
    return qGlobalPostedEventsCount() > 0;
}

bool SimpleEventDispatcher::registerEventNotifier(QWinEventNotifier* /* notifier */)
{
    Q_UNIMPLEMENTED();
    return false;
}

void SimpleEventDispatcher::registerSocketNotifier(QSocketNotifier* /* notifier */)
{
    Q_UNIMPLEMENTED();
}

void SimpleEventDispatcher::registerTimer(
    int /* timer_td */, int /* interval */, Qt::TimerType /* timer_type */, QObject* /* object */)
{
    Q_UNIMPLEMENTED();
}

int SimpleEventDispatcher::remainingTime(int /* timer_id */)
{
    Q_UNIMPLEMENTED();
    return 0;
}

void SimpleEventDispatcher::unregisterEventNotifier(QWinEventNotifier* /* notifier */)
{
    Q_UNIMPLEMENTED();
}

void SimpleEventDispatcher::unregisterSocketNotifier(QSocketNotifier* /* notifier */)
{
    Q_UNIMPLEMENTED();
}

bool SimpleEventDispatcher::unregisterTimer(int /* timer_id */)
{
    Q_UNIMPLEMENTED();
    return false;
}

bool SimpleEventDispatcher::unregisterTimers(QObject* /* object */)
{
    Q_UNIMPLEMENTED();
    return false;
}

QList<SimpleEventDispatcher::TimerInfo> SimpleEventDispatcher::registeredTimers(QObject* object) const
{
    Q_UNIMPLEMENTED();
    return QList<TimerInfo>();
}

void SimpleEventDispatcher::wakeUp()
{
    QMutexLocker lock(&event_lock_);
    awaken_ = true;
    event_.wakeAll();
}

void SimpleEventDispatcher::interrupt()
{
    interrupt_ = true;
    wakeUp();
}

void SimpleEventDispatcher::flush()
{
    // Nothing
}

} // namespace aspia
