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
#include <asio/steady_timer.hpp>

#if defined(Q_OS_WINDOWS)
#include <asio/windows/object_handle.hpp>
#else

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

    void registerTimer(int timer_id, qint64 interval, Qt::TimerType type, QObject* object) final;
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

    struct TimerData
    {
        Milliseconds interval;
        Qt::TimerType type;
        QObject* object;
        TimePoint start_time;
        TimePoint end_time;
    };

    struct SocketData
    {
#if defined(Q_OS_WINDOWS)
        using Handle = asio::windows::object_handle;
#else
        using Handle = asio::posix::stream_descriptor;
#endif
        explicit SocketData(Handle handle)
            : handle(std::move(handle))
        {
            // Nothing
        }

        QSocketNotifier* read = nullptr;
        QSocketNotifier* write = nullptr;
        QSocketNotifier* exception = nullptr;
        Handle handle;
    };

    Q_ALWAYS_INLINE void scheduleNextTimer();
    Q_ALWAYS_INLINE void ayncWaitForSocketEvent(qintptr socket, SocketData::Handle& handle);

#if defined(Q_OS_WINDOWS)
    // Returns |false| if execution should be aborted (the notifier no longer exists).
    Q_ALWAYS_INLINE bool sendSocketEvent(
        qintptr socket, long events, long mask, QSocketNotifier* notifier, QEvent::Type type);
#endif // defined(Q_OS_WINDOWS)

    using Timers = std::unordered_map<int, TimerData>;
    using TimersConstIterator = Timers::const_iterator;
    using Sockets = std::unordered_map<qintptr, SocketData>;
    using SocketsConstIterator = Sockets::const_iterator;

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic_bool interrupted_ { false };
    asio::steady_timer timer_;

    Timers timers_;
    TimersConstIterator timers_end_;
    Sockets sockets_;
    SocketsConstIterator sockets_end_;

    Q_DISABLE_COPY(AsioEventDispatcher)
};

} // namespace base

#endif // BASE_ASIO_EVENT_DISPATCHER_H
