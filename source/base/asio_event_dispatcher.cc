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
#include <QThread>

#include <asio/post.hpp>

#include "base/logging.h"

namespace base {

namespace {

const size_t kReservedSizeForTimersMap = 50;
const float kLoadFactorForTimersMap = 0.5;

} // namespace

//--------------------------------------------------------------------------------------------------
AsioEventDispatcher::AsioEventDispatcher(QObject* parent)
    : QAbstractEventDispatcher(parent),
      work_guard_(asio::make_work_guard(io_context_)),
      timer_(io_context_)
{
    LOG(INFO) << "Ctor";
    timers_.reserve(kReservedSizeForTimersMap);
    timers_.max_load_factor(kLoadFactorForTimersMap);
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

    emit awake();
    QCoreApplication::sendPostedEvents();

    // When calling method sendPostedEvents, the state of variable may change, so we check it.
    if (interrupted_.load(std::memory_order_relaxed))
        return false;

    size_t total_count = 0;
    size_t current_count = 0;

    do
    {
        io_context_.restart();
        current_count = io_context_.poll();

        QCoreApplication::sendPostedEvents();

        if (flags.testFlag(QEventLoop::WaitForMoreEvents) &&
            !interrupted_.load(std::memory_order_relaxed) &&
            !current_count)
        {
            emit aboutToBlock();
            io_context_.restart();
            current_count = io_context_.run_one();
            emit awake();
        }

        total_count += current_count;
    }
    while (!interrupted_.load(std::memory_order_relaxed) && current_count);

    return total_count != 0;
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
    int timer_id, qint64 interval, Qt::TimerType type, QObject* object)
{
    if (!object)
        return;

    const TimePoint start_time = Clock::now();
    const TimePoint end_time = start_time + Milliseconds(interval);

    timers_.emplace(std::make_pair(
        timer_id, TimerData(Milliseconds(interval), type, object, start_time, end_time)));

    scheduleNextTimer();
}

//--------------------------------------------------------------------------------------------------
bool AsioEventDispatcher::unregisterTimer(int timer_id)
{
    auto it = timers_.find(timer_id);
    if (it == timers_.end())
        return false;

    timers_.erase(it);

    if (!timers_.empty())
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

    if (!timers_.empty())
        scheduleNextTimer();

    return true;
}

//--------------------------------------------------------------------------------------------------
QList<QAbstractEventDispatcher::TimerInfo> AsioEventDispatcher::registeredTimers(QObject* object) const
{
    QList<TimerInfo> list;

    for (auto it = timers_.cbegin(), it_end = timers_.cend(); it != it_end; ++it)
    {
        const TimerData& timer = it->second;
        if (timer.object == object)
            list.append({ it->first, static_cast<int>(timer.interval.count()), timer.type });
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
int AsioEventDispatcher::remainingTime(int timer_id)
{
    const auto& it = timers_.find(timer_id);
    if (it == timers_.cend())
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
    // Find the timer that should be completed before all others.
    const auto& next_expire_timer = std::min_element(timers_.cbegin(), timers_.cend(),
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
        if (it == timers_.end())
            return;

        QTimerEvent event(timer_id);
        QCoreApplication::sendEvent(it->second.object, &event);

        // When calling method sendEvent the timer may have been deleted.
        it = timers_.find(timer_id);
        if (it == timers_.end())
            return;

        TimerData& timer = it->second;
        timer.start_time = timer.end_time;
        timer.end_time += timer.interval;

        scheduleNextTimer();
    });
}

} // namespace base
