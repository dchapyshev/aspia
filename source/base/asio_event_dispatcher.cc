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

const size_t kReservedSizeForTimersMap = 256;
const float kLoadFactorForTimersMap = 0.5;
const size_t kReservedSizeForSocketsMap = 256;
const float kLoadFactorForSocketsMap = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_)),
      timer_(io_context_),
      timers_end_(timers_.cend()),
      sockets_end_(sockets_.cend())
{
    LOG(INFO) << "Ctor";
    timers_.reserve(kReservedSizeForTimersMap);
    timers_.max_load_factor(kLoadFactorForTimersMap);
    sockets_.reserve(kReservedSizeForSocketsMap);
    sockets_.max_load_factor(kLoadFactorForSocketsMap);
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

        static const long kEventMask = FD_READ | FD_ACCEPT | FD_WRITE | FD_CONNECT | FD_OOB | FD_CLOSE;

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
    }

    SocketData& data = it->second;

    if (type == QSocketNotifier::Read)
    {
        data.read = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_read);
#endif
    }
    else if (type == QSocketNotifier::Write)
    {
        data.write = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_write);
#endif
    }
    else
    {
        data.exception = notifier;
#if defined(Q_OS_UNIX)
        asyncWaitForSocketEvent(data.handle, SocketData::Handle::wait_error);
#endif
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
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerTimer(
    int timer_id, qint64 interval, Qt::TimerType type, QObject* object)
{
    if (!object)
        return;

    const TimePoint start_time = Clock::now();
    const TimePoint end_time = start_time + Milliseconds(interval);

    timers_.emplace(timer_id, TimerData(Milliseconds(interval), type, object, start_time, end_time));
    timers_end_ = timers_.cend();

    scheduleNextTimer();
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
    auto it = timers_.find(timer_id);
    if (it == timers_end_)
        return false;

    timers_.erase(it);
    timers_end_ = timers_.cend();

    scheduleNextTimer();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    auto it = timers_.begin();
    bool removed = false;

    while (it != timers_.end())
    {
        if (it->second.object == object)
        {
            it = timers_.erase(it);
            removed = true;
        }
        else
        {
            ++it;
        }
    }

    if (!removed)
        return false;

    timers_end_ = timers_.cend();
    scheduleNextTimer();
    return true;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    for (auto it = timers_.cbegin(); it != timers_end_; ++it)
    {
        const TimerData& timer = it->second;
        if (timer.object == object)
            list.emplace_back(it->first, static_cast<int>(timer.interval.count()), timer.type);
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int timer_id)
{
    const auto& it = timers_.find(timer_id);
    if (it == timers_end_)
        return -1;

    const TimerData& timer = it->second;
    const Milliseconds elapsed = std::chrono::duration_cast<Milliseconds>(
        Clock::now() - timer.start_time);

    return static_cast<int>((timer.interval - elapsed).count());
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
void AsioEventDispatcher::scheduleNextTimer()
{
    if (timers_.empty())
        return;

    // Find the timer that should be completed before all others.
    const auto& next_expire_timer = std::min_element(timers_.cbegin(), timers_end_,
        [](const auto& lhs, const auto& rhs)
    {
        return lhs.second.end_time < rhs.second.end_time;
    });

    const int timer_id = next_expire_timer->first;

    // Start waiting for the timer.
    timer_.expires_at(next_expire_timer->second.end_time);
    timer_.async_wait([this, timer_id](const std::error_code& error_code)
    {
        if (error_code)
            return;

        auto it = timers_.find(timer_id);
        if (it == timers_end_)
            return;

        TimersConstIterator old_end = timers_end_;
        TimerData& timer = it->second;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(timer.object, &event);

        // When calling method sendEvent the timer may have been deleted.
        if (old_end != timers_end_ && !timers_.contains(timer_id))
        {
            scheduleNextTimer();
            return;
        }

        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        scheduleNextTimer();
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

        if (!sendSocketEvent(socket, events, FD_READ | FD_ACCEPT, data.read, QEvent::SockAct))
            return;

        if (!sendSocketEvent(socket, events, FD_WRITE | FD_CONNECT, data.write, QEvent::SockAct))
            return;

        if (!sendSocketEvent(socket, events, FD_OOB, data.exception, QEvent::SockAct))
            return;

        if (!sendSocketEvent(socket, events, FD_CLOSE, data.read, QEvent::SockClose))
            return;

        ayncWaitForSocketEvent(socket, data.handle);
    });
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::sendSocketEvent(
    qintptr socket, long events, long mask, QSocketNotifier* notifier, QEvent::Type type)
{
    if (!(events & mask) || !notifier)
    {
        // There are no notifications for this event type or there is no notifier for this event type.
        return true;
    }

    // To avoid searching the list on each iteration, we save the old end of the list. If the
    // end has changed, the list has changed and we check for the presence of the socket.
    SocketsConstIterator old_end = sockets_end_;

    QEvent event(type);
    QCoreApplication::sendEvent(notifier, &event);

    if (old_end == sockets_end_)
    {
        // The socket list has not changed.
        return true;
    }

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
            return;

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

        // To avoid searching the list on each iteration, we save the old end of the list. If
        // the end has changed, the list has changed and we check for the presence of the socket.
        SocketsConstIterator old_end = sockets_end_;

        QEvent event(QEvent::SockAct);
        QCoreApplication::sendEvent(notifier, &event);

        if (old_end != sockets_end_ && !sockets_.contains(socket))
            return;

        asyncWaitForSocketEvent(data.handle, wait_type);
    });
}
#endif // defined(Q_OS_UNIX)

} // namespace base
