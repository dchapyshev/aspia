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

const size_t kReservedSizeForZeroTimers = 32;

#if defined(Q_OS_WINDOWS)
const size_t kReservedSizeForMultimediaTimers = 16;
const float kLoadFactorForMultimediaTimers = 0.5;
#endif

const size_t kReservedSizeForPreciseTimers = 64;
const float kLoadFactorForPreciseTimers = 0.5;

const size_t kReservedSizeForCoarseTimers = 128;
const float kLoadFactorForCoarseTimers = 0.5;

const size_t kReservedSizeForSockets = 256;
const float kLoadFactorForSockets = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_))
{
    zero_timers_.reserve(kReservedSizeForZeroTimers);

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

    QCoreApplication::sendPostedEvents();

    // When calling method sendPostedEvents, the state of variable may change, so we check it.
    if (interrupted_.load(std::memory_order_relaxed))
        return false;

    size_t total_count = 0;
    size_t current_count = 0;

    do
    {
        QCoreApplication::sendPostedEvents();

        // Processing of Zero timers should be performed after QCoreApplication::sendPostedEvents
        // and before I/O and regular timers.
        while (!zero_timers_.empty())
        {
            auto first_timer = zero_timers_.begin();

            QTimerEvent event(first_timer->first);
            QCoreApplication::sendEvent(first_timer->second, &event);

            zero_timers_.erase(first_timer);
            ++current_count;
        }

        current_count += io_context_.poll();

        if (flags.testFlag(QEventLoop::WaitForMoreEvents) &&
            !interrupted_.load(std::memory_order_relaxed) &&
            !current_count)
        {
            emit aboutToBlock();
            current_count = io_context_.run_one();
            emit awake();
        }

        total_count += current_count;
        current_count = 0;
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
    bool is_new_socket = false;

    auto it = sockets_.find(socket);
    if (it == sockets_.end())
    {
        is_new_socket = true;

#if defined(Q_OS_WINDOWS)
        WSAEVENT wsa_event = WSACreateEvent();
        if (wsa_event == WSA_INVALID_EVENT)
        {
            LOG(ERROR) << "WSACreateEvent failed:" << WSAGetLastError();
            return;
        }

        // In Windows we subscribe to all events at once. If the notifier for a specific event is
        // not registered, it is ignored.
        const long kEventMask = FD_READ | FD_ACCEPT | FD_WRITE | FD_CONNECT | FD_OOB | FD_CLOSE;

        if (WSAEventSelect(socket, wsa_event, kEventMask) == SOCKET_ERROR)
        {
            LOG(ERROR) << "WSAEventSelect failed:" << WSAGetLastError();
            WSACloseEvent(wsa_event);
            return;
        }

        SocketData data(SocketHandle(io_context_, wsa_event));
#else
        SocketData data(SocketHandle(io_context_, socket))
#endif

        it = sockets_.emplace(socket, std::move(data)).first;
        sockets_changed_ = true;
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
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (is_new_socket)
        asyncWaitSocket(socket, data.handle);
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

    if (data.read != notifier && data.write != notifier && data.exception != notifier)
        return;

    switch (notifier->type())
    {
        case QSocketNotifier::Read:
            data.read = nullptr;
            break;

        case QSocketNotifier::Write:
            data.write = nullptr;
            break;

        default:
            data.exception = nullptr;
            break;
    }

    if (data.read || data.write || data.exception)
    {
        // There are active notifiers for this socket.
        return;
    }

    std::error_code ignored_error;
    data.handle.cancel(ignored_error);

#if defined(Q_OS_WINDOWS)
    if (data.handle.native_handle() != WSA_INVALID_EVENT)
    {
        WSAEventSelect(socket, nullptr, 0);
        WSACloseEvent(data.handle.native_handle());
    }
#endif

    sockets_.erase(it);
    sockets_changed_ = true;
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerTimer(
    int timer_id, qint64 interval_ms, Qt::TimerType type, QObject* object)
{
    if (!object)
        return;

    // Optimization for single-shot zero timers.
    if (interval_ms == 0)
    {
        zero_timers_.emplace(timer_id, object);
        wakeUp();
        return;
    }

    // Precision timers have high overhead resources. If the timer has a large interval, then we
    // force it to be coarse.
    if (type == Qt::PreciseTimer && interval_ms > 100)
        type = Qt::CoarseTimer;

    Milliseconds interval(interval_ms);
    TimePoint start_time = Clock::now();

    if (type == Qt::PreciseTimer)
    {
        TimePoint end_time = start_time + interval;

#if defined(Q_OS_WINDOWS)
        // In Windows asio::high_resolution_timer is equivalent to asio::steady_timer and therefore
        // only provides accuracy within 15ms. Multimedia timers provide 1ms accuracy, but their
        // availability is very limited. We try to create no more than the quantity specified in
        // kReservedSizeForMultimediaTimers. If the number of timers is greater or it is not possible
        // to create a multimedia timer, then we create asio::high_resolution_timer timer.
        while (multimedia_timers_.size() <= kReservedSizeForMultimediaTimers)
        {
            HANDLE event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!event_handle)
                break;

            asio::windows::object_handle handle(io_context_, event_handle);

            // TODO: Use CreateWaitableTimerExW with flag CREATE_WAITABLE_TIMER_HIGH_RESOLUTION after
            // Windows 7/8 support ends.
            const UINT flags = TIME_PERIODIC | TIME_CALLBACK_EVENT_SET;
            quint32 native_id = timeSetEvent(
                interval_ms, 1, reinterpret_cast<LPTIMECALLBACK>(event_handle), 0, flags);
            if (!native_id)
                break;

            auto timer_it = multimedia_timers_.emplace(timer_id, MultimediaTimer(
                std::move(handle), native_id, interval, type, object, start_time, end_time)).first;
            multimedia_timers_changed_ = true;

            asyncWaitMultimediaTimer(timer_it->second.event_handle, timer_id);
            return;
        }
#endif

        auto timer_it = precise_timers_.emplace(timer_id, PreciseTimer(
            asio::high_resolution_timer(io_context_), interval, type, object, start_time, end_time)).first;
        precise_timers_changed_ = true;

        asyncWaitPreciseTimer(timer_it->second.handle, timer_id, end_time);
    }
    else if (type == Qt::CoarseTimer || type == Qt::VeryCoarseTimer)
    {
        if (type == Qt::VeryCoarseTimer)
        {
            // Very coarse timers should wake up the thread as infrequently as possible, so their
            // interval cannot be lower than 1 second and should be rounded to the nearest second.
            // This allows timers to fire less frequently and allows multiple timers to fire at the
            // same time.
            interval = (interval < Milliseconds(1000)) ? Seconds(1) : std::chrono::round<Seconds>(interval);
            start_time = std::chrono::floor<Seconds>(start_time);
        }

        TimePoint end_time = start_time + interval;

        auto timer_it = coarse_timers_.emplace(timer_id, CoarseTimer(
            asio::steady_timer(io_context_), interval, type, object, start_time, end_time)).first;
        coarse_timers_changed_ = true;

        asyncWaitCoarseTimer(timer_it->second.handle, timer_id, end_time);
    }
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
    if (zero_timers_.erase(timer_id) != 0)
        return true;

#if defined(Q_OS_WINDOWS)
    auto it = multimedia_timers_.find(timer_id);
    if (it != multimedia_timers_.end())
    {
        timeKillEvent(it->second.native_id);
        multimedia_timers_.erase(it);

        multimedia_timers_changed_ = true;
        return true;
    }
#endif

    if (precise_timers_.erase(timer_id) != 0)
    {
        precise_timers_changed_ = true;
        return true;
    }

    if (coarse_timers_.erase(timer_id) != 0)
    {
        coarse_timers_changed_ = true;
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    bool removed = false;

    removed |= std::erase_if(
        zero_timers_, [object](const auto& timer) { return timer.second == object; }) != 0;

#if defined(Q_OS_WINDOWS)
    for (auto it = multimedia_timers_.begin(); it != multimedia_timers_.end();)
    {
        if (it->second.object == object)
        {
            timeKillEvent(it->second.native_id);
            it = multimedia_timers_.erase(it);

            multimedia_timers_changed_ = true;
            removed = true;
        }
        else
        {
            ++it;
        }
    }
#endif

    if (std::erase_if(precise_timers_,
        [object](const auto& timer) { return timer.second.object == object; }) != 0)
    {
        precise_timers_changed_ = true;
        removed = true;
    }

    if (std::erase_if(coarse_timers_,
        [object](const auto& timer) { return timer.second.object == object; }) != 0)
    {
        coarse_timers_changed_ = true;
        removed = true;
    }

    return removed;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    auto add_timers = [&](const auto& timers)
    {
        for (const auto& [id, timer] : timers)
        {
            if (timer.object == object)
                list.emplace_back(id, static_cast<int>(timer.interval.count()), timer.type);
        }
    };

#if defined(Q_OS_WINDOWS)
    add_timers(multimedia_timers_);
#endif

    add_timers(precise_timers_);
    add_timers(coarse_timers_);

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

        const Milliseconds elapsed =
            std::chrono::duration_cast<Milliseconds>(now - it->second.start_time);
        return static_cast<int>((it->second.interval - elapsed).count());
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

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        precise_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (precise_timers_changed_ && !precise_timers_.contains(timer_id))
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

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        coarse_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (coarse_timers_changed_ && !coarse_timers_.contains(timer_id))
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

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        multimedia_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (multimedia_timers_changed_ && !multimedia_timers_.contains(timer_id))
            return;

        asyncWaitMultimediaTimer(timer.event_handle, timer_id);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitSocket(qintptr socket, SocketHandle& handle)
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

        if (!sendSocketEvent(data.read, socket, events, FD_READ | FD_ACCEPT | FD_CLOSE))
            return;

        if (!sendSocketEvent(data.write, socket, events, FD_WRITE | FD_CONNECT))
            return;

        if (!sendSocketEvent(data.exception, socket, events, FD_OOB))
            return;

        asyncWaitSocket(socket, data.handle);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::sendSocketEvent(
    QSocketNotifier* notifier, qintptr socket, long events, long mask)
{
    // There are no notifications for this event type or there is no notifier for this event type.
    if (!(events & mask) || !notifier)
        return true;

    sockets_changed_ = false;

    QEvent event(QEvent::SockAct);
    QCoreApplication::sendEvent(notifier, &event);

    // The socket list may change during call sendEvent.
    if (!sockets_changed_)
        return true;

    // If the list has changed, we try to find the socket in it. If there is no socket, then further
    // execution should be interrupted and the next asynchronous wait will not be called.
    return sockets_.contains(socket);
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
        {
            // The socket is no longer in the list. The next async wait for it will not be called.
            return;
        }

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

        sockets_changed_ = false;

        QEvent event(QEvent::SockAct);
        QCoreApplication::sendEvent(notifier, &event);

        // The socket list may change during call sendEvent. If the list has changed, we try to find
        // the socket in it. If there is no socket, then further execution should be interrupted and
        // the next asynchronous wait will not be called.
        if (sockets_changed_ && !sockets_.contains(socket))
            return;

        asyncWaitSocket(data.handle, wait_type);
    });
}
#endif // defined(Q_OS_UNIX)

} // namespace base
