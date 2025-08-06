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

#include "base/logging.h"

namespace base {

namespace {

const size_t kReservedSizeForPreciseTimers = 8;

const size_t kReservedSizeForCoarseTimers = 64;
const float kLoadFactorForCoarseTimers = 0.5;

const size_t kReservedSizeForVeryCoarseTimers = 1024;
const float kLoadFactorForVeryCoarseTimers = 0.5;

const size_t kReservedSizeForSockets = 256;
const float kLoadFactorForSockets = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_))
{
    LOG(INFO) << "Ctor";

    precise_timers_.reserve(kReservedSizeForPreciseTimers);

    coarse_timers_.reserve(kReservedSizeForCoarseTimers);
    coarse_timers_.max_load_factor(kLoadFactorForCoarseTimers);

    very_coarse_timers_.reserve(kReservedSizeForVeryCoarseTimers);
    very_coarse_timers_.max_load_factor(kLoadFactorForVeryCoarseTimers);

    sockets_.reserve(kReservedSizeForSockets);
    sockets_.max_load_factor(kLoadFactorForSockets);
}

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::~AsioEventDispatcher()
{
    LOG(INFO) << "Dtor";
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
        current_count = io_context_.poll();

        QCoreApplication::sendPostedEvents();

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
        asyncWaitForSocketEvent(data.handle, SocketHandle::wait_read);
#endif
    }
    else if (type == QSocketNotifier::Write && !data.write)
    {
        data.write = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketHandle::wait_write);
#endif
    }
    else if (type == QSocketNotifier::Exception && !data.exception)
    {
        data.exception = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketHandle::wait_error);
#endif
    }
    else
    {
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (is_new_socket)
        ayncWaitForSocketEvent(socket, data.handle);
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

    if (type == Qt::PreciseTimer)
    {
        Milliseconds interval(interval_ms);
        TimePoint start_time = Clock::now();
        TimePoint end_time = start_time + interval;

        auto timer_it = precise_timers_.emplace(timer_id, PreciseTimerData(
            PreciseTimerHandle(io_context_), interval, type, object, start_time, end_time)).first;
        precise_timers_changed_ = true;

        schedulePreciseTimer(timer_it->second.handle, timer_id, end_time);
    }
    else if (type == Qt::CoarseTimer)
    {
        Milliseconds interval(interval_ms);
        TimePoint start_time = Clock::now();
        TimePoint end_time = start_time + interval;

        auto timer_it = coarse_timers_.emplace(timer_id, CoarseTimerData(
            CoarseTimerHandle(io_context_), interval, type, object, start_time, end_time)).first;
        coarse_timers_changed_ = true;

        scheduleCoarseTimer(timer_it->second.handle, timer_id, end_time);
    }
    else if (type == Qt::VeryCoarseTimer)
    {
        Seconds interval = (interval_ms < 1000) ?
            Seconds(1) : std::chrono::round<Seconds>(Milliseconds(interval_ms));
        TimePoint start_time = std::chrono::floor<Seconds>(Clock::now());
        TimePoint end_time = start_time + interval;

        auto timer_it = very_coarse_timers_.emplace(timer_id, CoarseTimerData(
            CoarseTimerHandle(io_context_), interval, type, object, start_time, end_time)).first;
        very_coarse_timers_changed_ = true;

        scheduleVeryCoarseTimer(timer_it->second.handle, timer_id, end_time);
    }
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
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

    if (very_coarse_timers_.erase(timer_id) != 0)
    {
        very_coarse_timers_changed_ = true;
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    bool removed = false;

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

    if (std::erase_if(very_coarse_timers_,
        [object](const auto& timer) { return timer.second.object == object; }) != 0)
    {
        very_coarse_timers_changed_ = true;
        removed = true;
    }

    return removed;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    for (auto it = precise_timers_.cbegin(); it != precise_timers_.cend(); ++it)
    {
        const PreciseTimerData& timer = it->second;
        if (timer.object == object)
            list.emplace_back(it->first, static_cast<int>(timer.interval.count()), timer.type);
    }

    for (auto it = coarse_timers_.cbegin(); it != coarse_timers_.cend(); ++it)
    {
        const CoarseTimerData& timer = it->second;
        if (timer.object == object)
            list.emplace_back(it->first, static_cast<int>(timer.interval.count()), timer.type);
    }

    for (auto it = very_coarse_timers_.cbegin(); it != very_coarse_timers_.cend(); ++it)
    {
        const CoarseTimerData& timer = it->second;
        if (timer.object == object)
            list.emplace_back(it->first, static_cast<int>(timer.interval.count()), timer.type);
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int timer_id)
{
    auto get_time = [](TimePoint start_time, Milliseconds interval)
    {
        const Milliseconds elapsed =
            std::chrono::duration_cast<Milliseconds>(Clock::now() - start_time);
        return static_cast<int>((interval - elapsed).count());
    };

    const auto& precise_it = precise_timers_.find(timer_id);
    if (precise_it != precise_timers_.end())
        return get_time(precise_it->second.start_time, precise_it->second.interval);

    const auto& coarse_it = coarse_timers_.find(timer_id);
    if (coarse_it != coarse_timers_.end())
        return get_time(coarse_it->second.start_time, coarse_it->second.interval);

    const auto& very_coarse_it = very_coarse_timers_.find(timer_id);
    if (very_coarse_it != very_coarse_timers_.end())
        return get_time(very_coarse_it->second.start_time, very_coarse_it->second.interval);

    return -1;
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
void AsioEventDispatcher::schedulePreciseTimer(
    PreciseTimerHandle& handle, int timer_id, TimePoint end_time)
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

        PreciseTimerData& timer = it->second;

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        precise_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (precise_timers_changed_ && !precise_timers_.contains(timer_id))
            return;

        schedulePreciseTimer(timer.handle, timer_id, timer.end_time);
    });
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::scheduleCoarseTimer(
    CoarseTimerHandle& handle, int timer_id, TimePoint end_time)
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

        CoarseTimerData& timer = it->second;

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        coarse_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (coarse_timers_changed_ && !coarse_timers_.contains(timer_id))
            return;

        scheduleCoarseTimer(timer.handle, timer_id, timer.end_time);
    });
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::scheduleVeryCoarseTimer(
    CoarseTimerHandle& handle, int timer_id, TimePoint end_time)
{
    // Start waiting for the timer.
    handle.expires_at(end_time);
    handle.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = very_coarse_timers_.find(timer_id);
        if (it == very_coarse_timers_.end())
            return;

        CoarseTimerData& timer = it->second;

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        very_coarse_timers_changed_ = false;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        if (very_coarse_timers_changed_ && !very_coarse_timers_.contains(timer_id))
            return;

        scheduleVeryCoarseTimer(timer.handle, timer_id, timer.end_time);
    });
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::ayncWaitForSocketEvent(qintptr socket, SocketHandle& handle)
{
    handle.async_wait([this, socket](const std::error_code& error_code)
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

        ayncWaitForSocketEvent(socket, data.handle);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::sendSocketEvent(
    QSocketNotifier* notifier, qintptr socket, long events, long mask)
{
    if (!(events & mask) || !notifier)
    {
        // There are no notifications for this event type or there is no notifier for this event type.
        return true;
    }

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
void AsioEventDispatcher::asyncWaitForSocketEvent(
    SocketHandle& handle, SocketHandle::wait_type wait_type)
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

        asyncWaitForSocketEvent(data.handle, wait_type);
    });
}
#endif // defined(Q_OS_UNIX)

} // namespace base
