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

#ifndef BASE_THREADING_ASIO_EVENT_DISPATCHER_H
#define BASE_THREADING_ASIO_EVENT_DISPATCHER_H

#include <QAbstractEventDispatcher>
#include <QEvent>

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#if defined(Q_OS_WINDOWS)
#include <asio/windows/object_handle.hpp>
#else
#include <asio/posix/stream_descriptor.hpp>
#endif

#include <atomic>
#include <unordered_map>

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

    static asio::io_context& ioContext();

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

#if defined(Q_OS_WINDOWS)
    struct MultimediaTimer
    {
        void cancel();

        quint64 unique_id;
        asio::windows::object_handle handle;
        quint32 native_id;
        Milliseconds interval;
        TimePoint end_time;
        QObject* object;
    };
    using MultimediaTimers = std::unordered_map<int, MultimediaTimer>;
    using SocketHandle = asio::windows::object_handle;
#else
    using SocketHandle = asio::posix::stream_descriptor;
#endif

    struct Timer
    {
        void cancel() { handle.cancel(); }

        quint64 unique_id;
        asio::steady_timer handle;
        Milliseconds interval;
        TimePoint end_time;
        QObject* object;
        Qt::TimerType type;
    };

    struct SocketData
    {
        explicit SocketData(quint64 unique_id, SocketHandle handle)
            : unique_id(unique_id),
              handle(std::move(handle))
        {
            // Nothing
        }

        SocketData(SocketData&&) = default;
        ~SocketData();

        void cancel();

#if !defined(Q_OS_WINDOWS)
        QSocketNotifier* notifierFor(SocketHandle::wait_type wait_type) const;
        bool& armedFor(SocketHandle::wait_type wait_type);
#endif

        quint64 unique_id;
        QSocketNotifier* read = nullptr;
        QSocketNotifier* write = nullptr;
        QSocketNotifier* exception = nullptr;

#if !defined(Q_OS_WINDOWS)
        // asio cannot cancel a single wait type on a descriptor, so a pending wait stays alive
        // when its notifier is disabled. These flags prevent arming a duplicate wait when the
        // notifier is re-enabled before the pending wait has completed.
        bool read_armed = false;
        bool write_armed = false;
        bool exception_armed = false;
#endif

        SocketHandle handle;
    };

    using Timers = std::unordered_map<int, Timer>;
    using Sockets = std::unordered_map<qintptr, SocketData>;

    void asyncWaitTimer(asio::steady_timer& handle, const TimePoint& end_time, int timer_id);

#if defined(Q_OS_WINDOWS)
    bool tryRegisterMultimediaTimer(
        int timer_id, Milliseconds interval, const TimePoint& end_time, QObject* object);
    void asyncWaitMultimediaTimer(asio::windows::object_handle& handle, int timer_id);
    void asyncWaitSocket(SocketHandle& handle, qintptr socket);
#else
    void asyncWaitSocket(SocketData& data, SocketHandle::wait_type wait_type);
#endif

    quint64 uniqueId();

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic_bool interrupted_ { false };
    std::atomic_bool wakeup_posted_ { false };
    quint64 counter_ = 0;

#if defined(Q_OS_WINDOWS)
    MultimediaTimers multimedia_timers_;
#endif

    Timers timers_;
    Sockets sockets_;

    Q_DISABLE_COPY_MOVE(AsioEventDispatcher)
};

#endif // BASE_THREADING_ASIO_EVENT_DISPATCHER_H
