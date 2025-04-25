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

#ifndef BASE_THREADING_ASIO_EVENT_DISPATCHER_H
#define BASE_THREADING_ASIO_EVENT_DISPATCHER_H

#include "base/macros_magic.h"

#include <QAbstractEventDispatcher>

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <unordered_map>

namespace base {

class AsioEventDispatcher final : public QAbstractEventDispatcher
{
    Q_OBJECT

public:
    explicit AsioEventDispatcher(QObject* parent = nullptr);
    ~AsioEventDispatcher() final;

    bool processEvents(QEventLoop::ProcessEventsFlags flags) final;
    bool hasPendingEvents() final;

    void registerSocketNotifier(QSocketNotifier* notifier) final;
    void unregisterSocketNotifier(QSocketNotifier* notifier) final;

    void registerTimer(int timer_id, int interval, Qt::TimerType type, QObject* object) final;
    bool unregisterTimer(int timer_id) final;
    bool unregisterTimers(QObject* object) final;
    QList<TimerInfo> registeredTimers(QObject* object) const final;
    int remainingTime(int timer_id) final;

#if defined(Q_OS_WIN)
    bool registerEventNotifier(QWinEventNotifier* notifier) final;
    void unregisterEventNotifier(QWinEventNotifier* notifier) final;
#endif // defined(Q_OS_WIN)

    void wakeUp() final;
    void interrupt() final;
    void flush() final;

    static asio::io_context& currentIoContext();
    asio::io_context& ioContext();

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;

    struct TimerData
    {
        int interval;
        Qt::TimerType type;
        QObject* object;
        TimePoint start_time;
        TimePoint end_time;
    };

#if defined(Q_OS_WINDOWS)
    struct EventData
    {
        QWinEventNotifier* notifier;
        HANDLE wait_handle;
    };

    static void CALLBACK eventCallback(PVOID parameter, BOOLEAN timer_or_wait_fired);

    std::vector<EventData> events_;
#endif // defined(Q_OS_WINDOWS)

    void asyncWaitForNextTimer();

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic_bool interrupted_ { false };
    std::unordered_map<int, TimerData> timers_;
    asio::steady_timer timer_;

    DISALLOW_COPY_AND_ASSIGN(AsioEventDispatcher);
};

} // namespace base

#endif // BASE_THREADING_ASIO_EVENT_DISPATCHER_H
