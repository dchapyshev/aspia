//
// SmartCafe Project
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
    using PreciseClock = std::chrono::high_resolution_clock;
    using PreciseTimePoint = PreciseClock::time_point;
    using CoarseClock = std::chrono::steady_clock;
    using CoarseTimePoint = CoarseClock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

#if defined(Q_OS_WINDOWS)
    struct MultimediaTimer
    {
        void cancel();

        asio::windows::object_handle handle;
        quint32 native_id;
        Milliseconds interval;
        PreciseTimePoint end_time;
        QObject* object;
    };
    using MultimediaTimers = std::unordered_map<int, MultimediaTimer>;
    using SocketHandle = asio::windows::object_handle;
#else
    using SocketHandle = asio::posix::stream_descriptor;
#endif

    template <typename TimerHandleType, typename TimePointType>
    struct GenericTimer
    {
        void cancel() { handle.cancel(); }

        TimerHandleType handle;
        Milliseconds interval;
        TimePointType end_time;
        QObject* object;
    };

    struct SocketData
    {
        explicit SocketData(SocketHandle handle)
            : handle(std::move(handle))
        {
            // Nothing
        }

        void cancel();

        QSocketNotifier* read = nullptr;
        QSocketNotifier* write = nullptr;
        QSocketNotifier* exception = nullptr;
        SocketHandle handle;
    };

    using PreciseTimer = GenericTimer<asio::high_resolution_timer, PreciseTimePoint>;
    using PreciseTimers = std::unordered_map<int, PreciseTimer>;
    using CoarseTimer = GenericTimer<asio::steady_timer, CoarseTimePoint>;
    using CoarseTimers = std::unordered_map<int, CoarseTimer>;
    using Sockets = std::unordered_map<qintptr, SocketData>;

    void asyncWaitPreciseTimer(
        asio::high_resolution_timer& handle, const PreciseTimePoint& end_time, int timer_id);
    void asyncWaitCoarseTimer(
        asio::steady_timer& handle, const CoarseTimePoint& end_time, int timer_id);

#if defined(Q_OS_WINDOWS)
    void asyncWaitMultimediaTimer(asio::windows::object_handle& handle, int timer_id);
    void asyncWaitSocket(SocketHandle& handle, qintptr socket);
#else
    void asyncWaitSocket(SocketHandle& handle, SocketHandle::wait_type wait_type);
#endif

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    std::atomic_bool interrupted_ { false };

#if defined(Q_OS_WINDOWS)
    MultimediaTimers multimedia_timers_;
#endif

    PreciseTimers precise_timers_;
    CoarseTimers coarse_timers_;
    Sockets sockets_;

    Q_DISABLE_COPY(AsioEventDispatcher)
};

} // namespace base

#endif // BASE_ASIO_EVENT_DISPATCHER_H
