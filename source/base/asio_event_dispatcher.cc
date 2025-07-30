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
const qint64 kWindowForVeryCoarseTimers = 1000;

const size_t kReservedSizeForSockets = 256;
const float kLoadFactorForSockets = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_)),
      precise_timer_(io_context_),
      coarse_timer_(io_context_),
      very_coarse_timer_(io_context_),
      precise_timers_end_(precise_timers_.cend()),
      coarse_timers_end_(coarse_timers_.cend()),
      very_coarse_timers_end_(very_coarse_timers_.cend()),
      sockets_end_(sockets_.cend())
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
    if (it == sockets_end_)
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

        SocketData data(SocketData::Handle(io_context_, wsa_event));
#else
        SocketData data(SocketData::Handle(io_context_, socket))
#endif

        it = sockets_.emplace(socket, std::move(data)).first;
        sockets_end_ = sockets_.cend();
        sockets_changed_ = true;
    }

    SocketData& data = it->second;

    if (type == QSocketNotifier::Read && !data.read)
    {
        data.read = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_read);
#endif
    }
    else if (type == QSocketNotifier::Write && !data.write)
    {
        data.write = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_write);
#endif
    }
    else if (type == QSocketNotifier::Exception && !data.exception)
    {
        data.exception = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_error);
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
    if (it == sockets_end_)
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
    sockets_end_ = sockets_.cend();
    sockets_changed_ = true;
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerTimer(
    int timer_id, qint64 interval_ms, Qt::TimerType type, QObject* object)
{
    if (!object)
        return;

    const Milliseconds interval(interval_ms);
    const TimePoint start_time = Clock::now();
    const TimePoint end_time = start_time + interval;

    if (type == Qt::PreciseTimer)
    {
        precise_timers_.emplace(timer_id, TimerData(interval, type, object, start_time, end_time));
        precise_timers_end_ = precise_timers_.cend();

        schedulePreciseTimer();
    }
    else if (type == Qt::CoarseTimer)
    {
        coarse_timers_.emplace(timer_id, TimerData(interval, type, object, start_time, end_time));
        coarse_timers_end_ = coarse_timers_.cend();

        scheduleCoarseTimer();
    }
    else if (type == Qt::VeryCoarseTimer)
    {
        bool schedule = very_coarse_timers_.empty();

        very_coarse_timers_.emplace(timer_id, TimerData(interval, type, object, start_time, end_time));
        very_coarse_timers_end_ = very_coarse_timers_.cend();
        very_coarse_timers_changed_ = true;

        if (schedule)
            scheduleVeryCoarseTimer();
    }
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
    auto remove_timer = [&](Timers& timers, TimersIterator& end)
    {
        if (timers.erase(timer_id) == 0)
            return false;
        end = timers.cend();
        return true;
    };

    if (remove_timer(precise_timers_, precise_timers_end_))
        return true;
    if (remove_timer(coarse_timers_, coarse_timers_end_))
        return true;

    if (remove_timer(very_coarse_timers_, very_coarse_timers_end_))
    {
        // If there are no more timers left in the list, then we stop the timer.
        if (very_coarse_timers_.empty())
            very_coarse_timer_.cancel();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    auto remove_timers = [object](Timers& timers, TimersIterator& end)
    {
        if (std::erase_if(timers, [&](const auto& timer) { return timer.second.object == object; }) == 0)
            return false;
        end = timers.cend();
        return true;
    };

    bool removed = false;

    removed |= remove_timers(precise_timers_, precise_timers_end_);
    removed |= remove_timers(coarse_timers_, coarse_timers_end_);

    if (remove_timers(very_coarse_timers_, very_coarse_timers_end_))
    {
        // If there are no more timers left in the list, then we stop the timer.
        if (very_coarse_timers_.empty())
            very_coarse_timer_.cancel();
        removed = true;
    }

    return removed;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    auto add_timers = [&](const Timers& timers, const TimersIterator& end)
    {
        for (auto it = timers.cbegin(); it != end; ++it)
        {
            const TimerData& timer = it->second;
            if (timer.object == object)
                list.emplace_back(it->first, static_cast<int>(timer.interval.count()), timer.type);
        }
    };

    add_timers(precise_timers_, precise_timers_end_);
    add_timers(coarse_timers_, coarse_timers_end_);
    add_timers(very_coarse_timers_, very_coarse_timers_end_);

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int timer_id)
{
    auto get_time = [timer_id](const Timers& timers, const TimersIterator& end)
    {
        const auto& it = timers.find(timer_id);
        if (it == end)
            return -1;

        const TimerData& timer = it->second;
        const Milliseconds elapsed = std::chrono::duration_cast<Milliseconds>(
            Clock::now() - timer.start_time);

        return static_cast<int>((timer.interval - elapsed).count());
    };

    int time = get_time(precise_timers_, precise_timers_end_);
    if (time != -1)
        return time;

    time = get_time(coarse_timers_, coarse_timers_end_);
    if (time != -1)
        return time;

    return get_time(very_coarse_timers_, very_coarse_timers_end_);
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
void AsioEventDispatcher::schedulePreciseTimer()
{
    if (precise_timers_.empty())
        return;

    // Find the timer that should be completed before all others.
    const auto& next_expire_timer = std::min_element(precise_timers_.cbegin(), precise_timers_end_,
        [](const auto& lhs, const auto& rhs)
    {
        return lhs.second.end_time < rhs.second.end_time;
    });

    const int timer_id = next_expire_timer->first;

    // Start waiting for the timer.
    precise_timer_.expires_at(next_expire_timer->second.end_time);
    precise_timer_.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = precise_timers_.find(timer_id);
        if (it != precise_timers_end_)
        {
            TimerData& timer = it->second;

            timer.start_time = timer.end_time;
            timer.end_time += timer.interval;

            QTimerEvent event(timer_id);
            QCoreApplication::sendEvent(timer.object, &event);
        }

        schedulePreciseTimer();
    });
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::scheduleCoarseTimer()
{
    if (coarse_timers_.empty())
        return;

    // Find the timer that should be completed before all others.
    const auto& next_expire_timer = std::min_element(coarse_timers_.cbegin(), coarse_timers_end_,
        [](const auto& lhs, const auto& rhs)
    {
        return lhs.second.end_time < rhs.second.end_time;
    });

    const int timer_id = next_expire_timer->first;

    // Start waiting for the timer.
    coarse_timer_.expires_at(next_expire_timer->second.end_time);
    coarse_timer_.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = coarse_timers_.find(timer_id);
        if (it != coarse_timers_end_)
        {
            TimerData& timer = it->second;

            timer.start_time = timer.end_time;
            timer.end_time += timer.interval;

            QTimerEvent event(timer_id);
            QCoreApplication::sendEvent(timer.object, &event);
        }

        scheduleCoarseTimer();
    });
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::scheduleVeryCoarseTimer()
{
    if (very_coarse_timers_.empty())
        return;

    // Start waiting for the timer.
    very_coarse_timer_.expires_after(Milliseconds(kWindowForVeryCoarseTimers));
    very_coarse_timer_.async_wait([this](const std::error_code& error_code)
    {
        if (error_code)
            return;

        TimePoint current_time = Clock::now();

        do
        {
            very_coarse_timers_changed_ = false;
            auto it = very_coarse_timers_.begin();

            while (it != very_coarse_timers_end_)
            {
                TimerData& timer = it->second;

                if (timer.end_time > current_time)
                {
                    ++it;
                    continue;
                }

                timer.start_time = timer.end_time;
                timer.end_time += timer.interval;

                QTimerEvent event(it->first);
                QCoreApplication::sendEvent(timer.object, &event);

                // As a result of calling method sendEvent, the list of timers may change.
                if (very_coarse_timers_changed_)
                {
                    // The list of timers has changed, iterators may be invalid.
                    break;
                }

                ++it;
            }
        }
        while (very_coarse_timers_changed_);

        scheduleVeryCoarseTimer();
    });
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::ayncWaitForSocketEvent(qintptr socket, SocketData::Handle& handle)
{
    handle.async_wait([this, socket](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = sockets_.find(socket);
        if (it == sockets_end_)
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
    SocketData::Handle& handle, SocketData::Handle::wait_type wait_type)
{
    const qintptr socket = handle.native_handle();

    handle.async_wait(wait_type, [this, socket, wait_type](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = sockets_.find(socket);
        if (it == sockets_end_)
        {
            // The socket is no longer in the list. The next async wait for it will not be called.
            return;
        }

        SocketData& data = it->second;
        QSocketNotifier* notifier;

        switch (wait_type)
        {
            case SocketData::Handle::wait_read:
                notifier = data.read;
                break;

            case SocketData::Handle::wait_write:
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
