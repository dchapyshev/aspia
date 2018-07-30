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

#ifndef ASPIA_BASE__SIMPLE_EVENT_DISPATCHER_H_
#define ASPIA_BASE__SIMPLE_EVENT_DISPATCHER_H_

#include <QAbstractEventDispatcher>
#include <QMutex>
#include <QWaitCondition>

#include "base/macros_magic.h"

namespace aspia {

class SimpleEventDispatcher : public QAbstractEventDispatcher
{
    Q_OBJECT

public:
    explicit SimpleEventDispatcher(QObject* parent = nullptr);
    ~SimpleEventDispatcher() = default;

    // Processes pending events that match |flags| until there are no more events to process.
    // Returns true if an event was processed; otherwise returns false.
    bool processEvents(QEventLoop::ProcessEventsFlags flags) override;

    // Returns true if there is an event waiting; otherwise returns false.
    bool hasPendingEvents() override;

    // Not implemented for this event dispatcher.
    void registerSocketNotifier(QSocketNotifier* notifier) override;
    void unregisterSocketNotifier(QSocketNotifier* notifier) override;

    // Register a timer with the specified |timer_id|, |interval|, and |timer_type| for the given |object|.
    void registerTimer(int timer_td, int interval, Qt::TimerType timer_type, QObject* object) override;

    // Returns the remaining time in milliseconds with the given |timer_id|.
    int remainingTime(int timer_id) override;

    // Unregisters the timer.
    bool unregisterTimer(int timer_id) override;

    // Unregisters all timers for the given |object|.
    bool unregisterTimers(QObject* object) override;

    QList<TimerInfo> registeredTimers(QObject* object) const override;

#if defined(Q_OS_WIN)
    // Not implemented for this event dispatcher.
    bool registerEventNotifier(QWinEventNotifier* notifier) override;
    void unregisterEventNotifier(QWinEventNotifier* notifier) override;
#endif

    // Wakes up the event loop. Thread-safe.
    void wakeUp() override;

    // Interrupts event dispatching. The event dispatcher will return from processEvents() as soon
    // as possible.
    void interrupt() override;

    // Does nothing.
    void flush() override;

private:
    QWaitCondition event_;
    QMutex event_lock_;

    bool awaken_;
    bool interrupt_;

    DISALLOW_COPY_AND_ASSIGN(SimpleEventDispatcher);
};

} // namespace aspia

#endif // ASPIA_BASE__SIMPLE_EVENT_DISPATCHER_H_
