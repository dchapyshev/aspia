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

#ifndef BASE_ASIO_EVENT_DISPATCHER_H
#define BASE_ASIO_EVENT_DISPATCHER_H

#include <QAbstractEventDispatcher>
#include <QEvent>

#include <asio/io_context.hpp>
#include <asio/high_resolution_timer.hpp>
#include <asio/steady_timer.hpp>

#if defined(Q_OS_WINDOWS)
#include <asio/windows/object_handle.hpp>
#else
#include <asio/posix/stream_descriptor.hpp>
#endif

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

    void registerSocketNotifier(QSocketNotifier* notifier) final;
    void unregisterSocketNotifier(QSocketNotifier* notifier) final;

    void registerTimer(int timer_id, qint64 interval_ms, Qt::TimerType type, QObject* object) final;
    bool unregisterTimer(int timer_id) final;
    bool unregisterTimers(QObject* object) final;
    QList<TimerInfo> registeredTimers(QObject* object) const final;
    int remainingTime(int timer_id) final;

    void wakeUp() final;
    void interrupt() final;

    static asio::io_context& currentIoContext();
    asio::io_context& ioContext();

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

#if defined(Q_OS_WINDOWS)
    using SocketHandle = asio::windows::object_handle;
#else
    using SocketHandle = asio::posix::stream_descriptor;
#endif

    using PreciseTimerHandle = asio::high_resolution_timer;
    using CoarseTimerHandle = asio::steady_timer;

    struct PreciseTimerData
    {
        PreciseTimerHandle handle;
        Milliseconds interval;
        Qt::TimerType type;
        QObject* object;
        TimePoint start_time;
        TimePoint end_time;
    };

    struct CoarseTimerData
    {
        CoarseTimerHandle handle;
        Milliseconds interval;
        Qt::TimerType type;
        QObject* object;
        TimePoint start_time;
        TimePoint end_time;
    };

    struct SocketData
    {
        explicit SocketData(SocketHandle handle)
            : handle(std::move(handle))
        {
            // Nothing
        }

        QSocketNotifier* read = nullptr;
        QSocketNotifier* write = nullptr;
        QSocketNotifier* exception = nullptr;
        SocketHandle handle;
    };

    void schedulePreciseTimer(PreciseTimerHandle& handle, int timer_id, TimePoint end_time);
    void scheduleCoarseTimer(CoarseTimerHandle& handle, int timer_id, TimePoint end_time);
    void scheduleVeryCoarseTimer(CoarseTimerHandle& handle, int timer_id, TimePoint end_time);

#if defined(Q_OS_WINDOWS)
    void ayncWaitForSocketEvent(qintptr socket, SocketHandle& handle);

    // Returns |false| if execution should be aborted (the notifier no longer exists).
    bool sendSocketEvent(QSocketNotifier* notifier, qintptr socket, long events, long mask);
#else
    void ayncWaitForSocketEvent(SocketHandle& handle, SocketHandle::wait_type wait_type);
#endif

    using PreciseTimers = std::unordered_map<int, PreciseTimerData>;
    using CoarseTimers = std::unordered_map<int, CoarseTimerData>;
    using Sockets = std::unordered_map<qintptr, SocketData>;

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic_bool interrupted_ { false };

    PreciseTimers precise_timers_;
    bool precise_timers_changed_ = false;

    CoarseTimers coarse_timers_;
    bool coarse_timers_changed_ = false;

    CoarseTimers very_coarse_timers_;
    bool very_coarse_timers_changed_ = false;

    Sockets sockets_;
    bool sockets_changed_ = false;

    Q_DISABLE_COPY(AsioEventDispatcher)
};

} // namespace base

#endif // BASE_ASIO_EVENT_DISPATCHER_H
