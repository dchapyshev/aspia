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

#include "base/asio_event_dispatcher.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#include <QThread>

#include <asio/post.hpp>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <Mmsystem.h>
#endif

#include "base/logging.h"

namespace base {

namespace {

#if defined(Q_OS_WINDOWS)
const size_t kReservedSizeForMultimediaTimers = 16;
const float kLoadFactorForMultimediaTimers = 0.5;
#endif

const size_t kReservedSizeForPreciseTimers = 64;
const float kLoadFactorForPreciseTimers = 0.5;

const size_t kReservedSizeForCoarseTimers = 256;
const float kLoadFactorForCoarseTimers = 0.5;

const size_t kReservedSizeForSockets = 256;
const float kLoadFactorForSockets = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_))
{
#if defined(Q_OS_WINDOWS)
    multimedia_timers_.reserve(kReservedSizeForMultimediaTimers);
    multimedia_timers_.max_load_factor(kLoadFactorForMultimediaTimers);
#endif

    precise_timers_.reserve(kReservedSizeForPreciseTimers);
    precise_timers_.max_load_factor(kLoadFactorForPreciseTimers);

    coarse_timers_.reserve(kReservedSizeForCoarseTimers);
    coarse_timers_.max_load_factor(kLoadFactorForCoarseTimers);

    sockets_.reserve(kReservedSizeForSockets);
    sockets_.max_load_factor(kLoadFactorForSockets);
}

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::~AsioEventDispatcher()
{
    work_guard_.reset();
    io_context_.stop();
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    interrupted_.store(false, std::memory_order_relaxed);

    emit awake();
    QCoreApplication::sendPostedEvents();

    // When calling method sendPostedEvents, the state of variable may change, so we check it.
    if (interrupted_.load(std::memory_order_relaxed))
        return false;

    size_t total_count = 0;
    size_t current_count;

    do
    {
        QCoreApplication::sendPostedEvents();
        current_count = io_context_.poll();

        if (flags.testFlag(QEventLoop::WaitForMoreEvents) &&
            !interrupted_.load(std::memory_order_relaxed) &&
            !current_count)
        {
            emit aboutToBlock();
            current_count = io_context_.run_one();
            emit awake();
        }

        total_count += current_count;
    }
    while (!interrupted_.load(std::memory_order_relaxed) && current_count);

    return total_count != 0;
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerSocketNotifier(QSocketNotifier* notifier)
{
    if (!notifier)
        return;

    const qintptr socket = notifier->socket();
    const QSocketNotifier::Type type = notifier->type();
    bool is_socket_added = false;

    auto it = sockets_.find(socket);
    if (it == sockets_.end())
    {
#if defined(Q_OS_WINDOWS)
        WSAEVENT wsa_event = WSACreateEvent();
        if (wsa_event == WSA_INVALID_EVENT)
        {
            LOG(ERROR) << "WSACreateEvent failed:" << WSAGetLastError();
            return;
        }

        // Now asio::windows::object_handle owns the handle.
        SocketHandle handle(io_context_, wsa_event);

        // In Windows we subscribe to all events at once. If the notifier for a specific event is
        // not registered, it is ignored.
        const long kEventMask = FD_READ | FD_ACCEPT | FD_WRITE | FD_CONNECT | FD_OOB | FD_CLOSE;

        if (WSAEventSelect(socket, wsa_event, kEventMask) == SOCKET_ERROR)
        {
            LOG(ERROR) << "WSAEventSelect failed:" << WSAGetLastError();
            return;
        }
#else
        SocketHandle handle(io_context_, socket);
#endif
        it = sockets_.emplace(socket, SocketData(std::move(handle))).first;
        is_socket_added = true;
    }

    SocketData& data = it->second;

    if (type == QSocketNotifier::Read && !data.read)
    {
        data.read = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitSocket(data.handle, SocketHandle::wait_read);
#endif
    }
    else if (type == QSocketNotifier::Write && !data.write)
    {
        data.write = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitSocket(data.handle, SocketHandle::wait_write);
#endif
    }
    else if (type == QSocketNotifier::Exception && !data.exception)
    {
        data.exception = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitSocket(data.handle, SocketHandle::wait_error);
#endif
    }
    else
    {
        LOG(ERROR) << "Multiple socket notifiers for" << socket;
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (is_socket_added)
        asyncWaitSocket(data.handle, socket);
#endif
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::unregisterSocketNotifier(QSocketNotifier* notifier)
{
    if (!notifier)
        return;

    const qintptr socket = notifier->socket();

    auto it = sockets_.find(socket);
    if (it == sockets_.end())
        return;

    SocketData& data = it->second;

    if (data.read == notifier)
        data.read = nullptr;
    else if (data.write == notifier)
        data.write = nullptr;
    else if (data.exception == notifier)
        data.exception = nullptr;
    else
        return;

    if (data.read || data.write || data.exception)
    {
        // There are active notifiers for this socket.
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (data.handle.native_handle() != WSA_INVALID_EVENT)
        WSAEventSelect(socket, nullptr, 0);
#endif

    sockets_.erase(it);
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerTimer(
    int timer_id, qint64 interval_ms, Qt::TimerType type, QObject* object)
{
    if (!object)
        return;

    Milliseconds interval(interval_ms);
    TimePoint start_time = Clock::now();

    // Precision timers have high overhead resources. If the timer has a large interval, then we
    // force it to be coarse.
    if (type == Qt::PreciseTimer && interval_ms > 100)
    {
        type = Qt::CoarseTimer;
    }
    else if (type == Qt::VeryCoarseTimer || interval_ms >= 20000)
    {
        // Very coarse timers should wake up the thread as infrequently as possible, so their
        // interval cannot be lower than 1 second and should be rounded to the nearest second.
        // This allows timers to fire less frequently and allows multiple timers to fire at the
        // same time.
        interval = std::chrono::ceil<Seconds>(interval);
        start_time = std::chrono::floor<Seconds>(start_time);
        type = Qt::CoarseTimer;
    }

    TimePoint end_time = start_time + interval;

    if (type == Qt::PreciseTimer)
    {
#if defined(Q_OS_WINDOWS)
        // In Windows asio::high_resolution_timer is equivalent to asio::steady_timer and therefore
        // only provides accuracy within 15ms. Multimedia timers provide 1ms accuracy, but their
        // availability is very limited. We try to create no more than the quantity specified in
        // kReservedSizeForMultimediaTimers. If the number of timers is greater or it is not possible
        // to create a multimedia timer, then we create asio::high_resolution_timer timer.
        while (multimedia_timers_.size() < kReservedSizeForMultimediaTimers)
        {
            HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!event)
            {
                PLOG(ERROR) << "CreateEventW failed";
                break;
            }

            // Now asio::windows::object_handle owns the handle.
            asio::windows::object_handle handle(io_context_, event);

            // TODO: Use CreateWaitableTimerExW with flag CREATE_WAITABLE_TIMER_HIGH_RESOLUTION after
            // Windows 7/8 support ends.
            const UINT flags = TIME_PERIODIC | TIME_CALLBACK_EVENT_SET;
            quint32 native_id = timeSetEvent(
                interval_ms, 1, reinterpret_cast<LPTIMECALLBACK>(event), 0, flags);
            if (!native_id)
            {
                // Normal case if there are too many multimedia timers registered.
                break;
            }

            auto timer = multimedia_timers_.emplace(timer_id, MultimediaTimer(
                std::move(handle), native_id, interval, end_time, object)).first;

            asyncWaitMultimediaTimer(timer->second.handle, timer_id);
            return;
        }
#endif
        auto timer = precise_timers_.emplace(timer_id, PreciseTimer(
            asio::high_resolution_timer(io_context_), interval, end_time, object)).first;

        asyncWaitPreciseTimer(timer->second.handle, timer_id, end_time);
    }
    else
    {
        auto timer = coarse_timers_.emplace(timer_id, CoarseTimer(
            asio::steady_timer(io_context_), interval, end_time, object)).first;

        asyncWaitCoarseTimer(timer->second.handle, timer_id, end_time);
    }
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
#if defined(Q_OS_WINDOWS)
    auto it = multimedia_timers_.find(timer_id);
    if (it != multimedia_timers_.end())
    {
        timeKillEvent(it->second.native_id);
        multimedia_timers_.erase(it);
        return true;
    }
#endif

    if (precise_timers_.erase(timer_id) != 0)
        return true;

    if (coarse_timers_.erase(timer_id) != 0)
        return true;

    return false;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    bool removed = false;

#if defined(Q_OS_WINDOWS)
    for (auto it = multimedia_timers_.begin(); it != multimedia_timers_.end();)
    {
        if (it->second.object == object)
        {
            timeKillEvent(it->second.native_id);
            it = multimedia_timers_.erase(it);
            removed = true;
        }
        else
        {
            ++it;
        }
    }
#endif

    removed |= std::erase_if(precise_timers_,
        [object](const auto& timer) { return timer.second.object == object; }) != 0;

    removed |= std::erase_if(coarse_timers_,
        [object](const auto& timer) { return timer.second.object == object; }) != 0;

    return removed;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    auto add_timers = [&](const auto& timers, Qt::TimerType type)
    {
        for (const auto& [id, timer] : timers)
        {
            if (timer.object == object)
                list.emplace_back(id, static_cast<int>(timer.interval.count()), type);
        }
    };

#if defined(Q_OS_WINDOWS)
    add_timers(multimedia_timers_, Qt::PreciseTimer);
#endif

    add_timers(precise_timers_, Qt::PreciseTimer);
    add_timers(coarse_timers_, Qt::CoarseTimer);

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int timer_id)
{
    TimePoint now = Clock::now();

    auto get_time = [&](const auto& timers) -> int
    {
        const auto& it = timers.find(timer_id);
        if (it == timers.end())
            return -1;

        if (now >= it->second.end_time)
            return 0;

        const Milliseconds remaining =
            std::chrono::duration_cast<Milliseconds>(it->second.end_time - now);

        return static_cast<int>(remaining.count());
    };

#if defined(Q_OS_WINDOWS)
    if (int time = get_time(multimedia_timers_); time != -1)
        return time;
#endif

    if (int time = get_time(precise_timers_); time != -1)
        return time;

    return get_time(coarse_timers_);
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::wakeUp()
{
    // To stop run_one inside method processEvents completes.
    asio::post(io_context_, []{});
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::interrupt()
{
    interrupted_.store(true, std::memory_order_relaxed);
    wakeUp();
}

//--------------------------------------------------------------------------------------------------
// static
asio::io_context& AsioEventDispatcher::currentIoContext()
{
    base::AsioEventDispatcher* dispatcher =
        dynamic_cast<base::AsioEventDispatcher*>(QThread::currentThread()->eventDispatcher());
    CHECK(dispatcher);

    return dispatcher->ioContext();
}

//--------------------------------------------------------------------------------------------------
asio::io_context& AsioEventDispatcher::ioContext()
{
    return io_context_;
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitPreciseTimer(
    asio::high_resolution_timer& handle, int timer_id, TimePoint end_time)
{
    // Start waiting for the timer.
    handle.expires_at(end_time);
    handle.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = precise_timers_.find(timer_id);
        if (it == precise_timers_.end())
            return;

        PreciseTimer& timer = it->second;
        timer.end_time += timer.interval;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (!precise_timers_.contains(timer_id))
            return;

        asyncWaitPreciseTimer(timer.handle, timer_id, timer.end_time);
    });
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitCoarseTimer(
    asio::steady_timer& handle, int timer_id, TimePoint end_time)
{
    // Start waiting for the timer.
    handle.expires_at(end_time);
    handle.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = coarse_timers_.find(timer_id);
        if (it == coarse_timers_.end())
            return;

        CoarseTimer& timer = it->second;
        timer.end_time += timer.interval;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (!coarse_timers_.contains(timer_id))
            return;

        asyncWaitCoarseTimer(timer.handle, timer_id, timer.end_time);
    });
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitMultimediaTimer(asio::windows::object_handle& handle, int timer_id)
{
    // Start waiting for the timer.
    handle.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = multimedia_timers_.find(timer_id);
        if (it == multimedia_timers_.end())
            return;

        MultimediaTimer& timer = it->second;
        timer.end_time += timer.interval;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (!multimedia_timers_.contains(timer_id))
            return;

        asyncWaitMultimediaTimer(timer.handle, timer_id);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitSocket(SocketHandle& handle, qintptr socket)
{
    handle.async_wait([this, socket](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = sockets_.find(socket);
        if (it == sockets_.end())
            return;

        SocketData& data = it->second;

        WSAEVENT wsa_event = data.handle.native_handle();
        WSANETWORKEVENTS network_events = {};

        if (WSAEnumNetworkEvents(socket, wsa_event, &network_events) == SOCKET_ERROR)
        {
            LOG(ERROR) << "WSAEnumNetworkEvents failed:" << WSAGetLastError();
            return;
        }

        const long events = network_events.lNetworkEvents;

        auto send_event = [&](QSocketNotifier* notifier, long mask) -> bool
        {
            // There are no notifications for this event type or there is no notifier for this event type.
            if (!(events & mask) || !notifier)
                return true;

            QEvent event(QEvent::SockAct);
            QCoreApplication::sendEvent(notifier, &event);

            // If the list has changed, we try to find the socket in it. If there is no socket, then further
            // execution should be interrupted and the next asynchronous wait will not be called.
            return sockets_.contains(socket);
        };

        if (!send_event(data.read, FD_READ | FD_ACCEPT | FD_CLOSE))
            return;

        if (!send_event(data.write, FD_WRITE | FD_CONNECT))
            return;

        if (!send_event(data.exception, FD_OOB))
            return;

        asyncWaitSocket(data.handle, socket);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitSocket(SocketHandle& handle, SocketHandle::wait_type wait_type)
{
    const qintptr socket = handle.native_handle();

    handle.async_wait(wait_type, [this, socket, wait_type](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = sockets_.find(socket);
        if (it == sockets_.end())
            return;

        SocketData& data = it->second;
        QSocketNotifier* notifier;

        switch (wait_type)
        {
            case SocketHandle::wait_read:
                notifier = data.read;
                break;

            case SocketHandle::wait_write:
                notifier = data.write;
                break;

            default:
                notifier = data.exception;
                break;
        }

        if (!notifier)
            return;

        QEvent event(QEvent::SockAct);
        QCoreApplication::sendEvent(notifier, &event);

        // The socket list may change during call sendEvent. If the list has changed, we try to find
        // the socket in it. If there is no socket, then further execution should be interrupted and
        // the next asynchronous wait will not be called.
        if (!sockets_.contains(socket))
            return;

        asyncWaitSocket(data.handle, wait_type);
    });
}
#endif // defined(Q_OS_UNIX)

} // namespace base
