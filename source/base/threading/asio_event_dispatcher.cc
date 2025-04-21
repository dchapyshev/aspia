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

#include "base/threading/asio_event_dispatcher.h"
#include "base/logging.h"

#include <asio/post.hpp>

#include <QCoreApplication>
#include <QThread>

#if defined(Q_OS_WIN)
#include <QWinEventNotifier>
#endif // defined(Q_OS_WIN)

namespace base {

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_)),
      timer_(io_context_)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::~AsioEventDispatcher()
{
    LOG(LS_INFO) << "Dtor";
    work_guard_.reset();
    io_context_.stop();
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    interrupted_.store(false, std::memory_order_relaxed);

    QCoreApplication::sendPostedEvents();

    if (interrupted_.load(std::memory_order_relaxed))
        return false;

    size_t total_count = 0;

    if (flags & QEventLoop::WaitForMoreEvents)
    {
        emit aboutToBlock();
        io_context_.restart();
        total_count += io_context_.run_one();
        emit awake();
    }

    while (true)
    {
        io_context_.restart();
        size_t count = io_context_.poll();

        QCoreApplication::sendPostedEvents();

        if (!count)
            break;

        total_count += count;
    }

    return total_count > 0;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount();
    return qGlobalPostedEventsCount() > 0;
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerSocketNotifier(QSocketNotifier* /* notifier */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::unregisterSocketNotifier(QSocketNotifier* /* notifier */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::registerTimer(
    int id, int interval, Qt::TimerType type, QObject* object)
{
    DCHECK(object);

    TimePoint start_time = Clock::now();
    TimePoint expire_time = start_time + Milliseconds(interval);

    timers_.emplace(
        std::make_pair(id, TimerData(id, interval, type, object, start_time, expire_time)));

    asyncWaitForNextTimer();
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int id)
{
    auto it = timers_.find(id);
    if (it == timers_.end())
        return false;

    timers_.erase(it);
    asyncWaitForNextTimer();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimers(QObject* object)
{
    DCHECK(object);

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

    if (removed)
        asyncWaitForNextTimer();

    return true;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(
    QObject* object) const
{
    DCHECK(object);

    QList<TimerInfo> list;

    for (auto it = timers_.cbegin(); it != timers_.cend(); ++it)
    {
        if (it->second.object == object)
            list.append({ it->second.id, it->second.interval, it->second.type });
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int id)
{
    auto it = timers_.find(id);
    if (it == timers_.end())
        return -1;

    uint64_t elapsed = std::chrono::duration_cast<Milliseconds>(
        Clock::now() - it->second.start_time).count();

    return it->second.interval - static_cast<int>(elapsed);
}

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_WIN)
bool AsioEventDispatcher::registerEventNotifier(QWinEventNotifier* notifier)
{
    DCHECK(notifier);

    HANDLE handle = notifier->handle();
    if (!handle || handle == INVALID_HANDLE_VALUE)
    {
        LOG(LS_ERROR) << "Invalid notifier handle";
        return false;
    }

    HANDLE wait_handle;
    if (!RegisterWaitForSingleObject(&wait_handle, handle, eventCallback, notifier, INFINITE,
                                     WT_EXECUTEINWAITTHREAD))
    {
        PLOG(LS_ERROR) << "RegisterWaitForSingleObject failed";
        return false;
    }

    events_.emplace_back(notifier, wait_handle);
    return true;
}
#endif // defined(Q_OS_WIN)

//--------------------------------------------------------------------------------------------------
#if defined(Q_OS_WIN)
void AsioEventDispatcher::unregisterEventNotifier(QWinEventNotifier* notifier)
{
    DCHECK(notifier);

    auto it = events_.begin();
    while (it != events_.end())
    {
        const EventData& event = *it;

        if (event.notifier == notifier)
        {
            UnregisterWait(event.wait_handle);
            it = events_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
#endif // defined(Q_OS_WIN)

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::wakeUp()
{
    asio::post(io_context_, []{});
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::interrupt()
{
    interrupted_.store(true, std::memory_order_relaxed);
    wakeUp();
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::flush()
{
    // Nothing
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
// static
#if defined(Q_OS_WIN)
void AsioEventDispatcher::eventCallback(PVOID parameter, BOOLEAN /* timer_or_wait_fired */)
{
    QWinEventNotifier* notifier = reinterpret_cast<QWinEventNotifier*>(parameter);
    if (!notifier)
    {
        LOG(LS_ERROR) << "Invalid pointer";
        return;
    }

    emit notifier->activated(notifier->handle(), {});
}
#endif // defined(Q_OS_WIN)

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::asyncWaitForNextTimer()
{
    if (timers_.empty())
        return;

    auto next_expire_timer = std::min_element(timers_.begin(), timers_.end(),
        [](const auto& lhs, const auto& rhs)
    {
        return lhs.second.expire_time < rhs.second.expire_time;
    });

    timer_.expires_at(next_expire_timer->second.expire_time);
    timer_.async_wait(std::bind(
        &AsioEventDispatcher::onTimerEvent, this, std::placeholders::_1, next_expire_timer->second.id));
}

//--------------------------------------------------------------------------------------------------
void AsioEventDispatcher::onTimerEvent(const std::error_code& error_code, int id)
{
    if (error_code || interrupted_.load(std::memory_order_relaxed))
        return;

    auto it = timers_.find(id);
    if (it == timers_.end())
        return;

    QCoreApplication::sendEvent(it->second.object, new QTimerEvent(id));

    it = timers_.find(id);
    if (it == timers_.end())
        return;

    TimerData& timer = it->second;
    timer.start_time = Clock::now();
    timer.expire_time = timer.start_time + Milliseconds(timer.interval);

    asyncWaitForNextTimer();
}

} // namespace base
