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

#include "base/threading/worker.h"

#include <QDeadlineTimer>
#include <QStringList>
#include <QTimerEvent>

#include <vector>

//--------------------------------------------------------------------------------------------------
// static
thread_local Worker* Worker::current_worker_ = nullptr;

//--------------------------------------------------------------------------------------------------
Worker::Worker(Thread::EventDispatcher dispatcher, MilliSeconds timer_interval)
    : thread_(dispatcher),
      timer_interval_(timer_interval)
{
    moveToThread(&thread_);
    connect(&thread_, &Thread::sig_beforeRunning, this, &Worker::onThreadStarted, Qt::DirectConnection);
    connect(&thread_, &Thread::sig_afterRunning, this, &Worker::onThreadFinished, Qt::DirectConnection);
}

//--------------------------------------------------------------------------------------------------
Worker::~Worker()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString Worker::name() const
{
    return metaObject()->className();
}

//--------------------------------------------------------------------------------------------------
void Worker::post(std::function<void()> work)
{
    QMetaObject::invokeMethod(this, std::move(work), Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void Worker::start(WorkerManager* manager)
{
    manager_ = manager;
    thread_.setObjectName(name());
    thread_.start();
}

//--------------------------------------------------------------------------------------------------
void Worker::stopSoon()
{
    thread_.quit();
}

//--------------------------------------------------------------------------------------------------
bool Worker::join(MilliSeconds timeout)
{
    return thread_.wait(QDeadlineTimer(timeout));
}

//--------------------------------------------------------------------------------------------------
void Worker::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == timer_id_)
    {
        const TimePoint now = Clock::now();
        onTimer(now);
        emit sig_tick(now);
    }
}

//--------------------------------------------------------------------------------------------------
void Worker::onThreadStarted()
{
    current_worker_ = this;

    onStart();

    if (timer_interval_ > MilliSeconds::zero())
        timer_id_ = startTimer(timer_interval_);

    std::lock_guard lock(manager_->lock_);
    started_ = true;
    manager_->condition_.notify_all();
}

//--------------------------------------------------------------------------------------------------
void Worker::onThreadFinished()
{
    if (timer_id_ != 0)
    {
        killTimer(timer_id_);
        timer_id_ = 0;
    }

    onStop();

    current_worker_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
WorkerManager::WorkerManager(QObject* parent)
    : QObject(parent),
      thread_id_(std::this_thread::get_id())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
WorkerManager::~WorkerManager()
{
    CHECK(std::this_thread::get_id() == thread_id_);

    LOG(INFO) << "Stopping workers...";

    if (watchdog_timer_id_ != 0)
    {
        killTimer(watchdog_timer_id_);
        watchdog_timer_id_ = 0;
    }

    std::vector<Worker*> pending;
    pending.reserve(workers_.size());

    for (const auto& worker : workers_)
    {
        pending.push_back(worker.second.get());
        worker.second->stopSoon();
    }

    // Wait until every worker thread has FULLY finished, including Qt's deferred-delete flush and
    // the destruction of the thread's event dispatcher. The threads are polled round-robin so one
    // stuck worker does not hide the state of the others; the full list of still-running workers
    // is reported periodically. Workers are destroyed strictly below, when ALL threads are done:
    // a live (even stuck) thread may still access its sibling workers.
    constexpr MilliSeconds kJoinPollInterval{ 50 };
    constexpr MilliSeconds kJoinWarnInterval = Seconds(3);

    TimePoint last_warn_time = Clock::now();
    while (!pending.empty())
    {
        for (auto it = pending.begin(); it != pending.end();)
        {
            if ((*it)->join(kJoinPollInterval))
                it = pending.erase(it);
            else
                ++it;
        }

        if (!pending.empty() && Clock::now() - last_warn_time >= kJoinWarnInterval)
        {
            QStringList names;
            for (Worker* worker : pending)
                names.append(worker->name());

            LOG(ERROR) << "Worker threads have not finished yet:" << names.join(", ");
            last_warn_time = Clock::now();
        }
    }

    LOG(INFO) << "All workers stopped";

    workers_.clear();

    LOG(INFO) << "All workers destroyed";
}

//--------------------------------------------------------------------------------------------------
qint64 WorkerManager::add(std::unique_ptr<Worker> worker)
{
    CHECK(std::this_thread::get_id() == thread_id_);
    CHECK(!started_);
    CHECK(worker);
    CHECK(!worker->parent());

    ++next_worker_id_;

    workers_.emplace(next_worker_id_, std::move(worker));
    return next_worker_id_;
}

//--------------------------------------------------------------------------------------------------
void WorkerManager::start()
{
    CHECK(std::this_thread::get_id() == thread_id_);
    CHECK(!started_);

    LOG(INFO) << "Starting workers...";

    for (const auto& worker : workers_)
        worker.second->start(this);

    constexpr MilliSeconds kStartWarnInterval = Seconds(3);

    {
        std::unique_lock lock(lock_);

        bool report = false;
        for (;;)
        {
            QStringList pending;
            for (const auto& worker : workers_)
            {
                if (!worker.second->started_)
                    pending.append(worker.second->name());
            }

            if (pending.isEmpty())
                break;

            if (report)
                LOG(ERROR) << "Worker threads have not started yet:" << pending.join(", ");

            report = (condition_.wait_for(lock, kStartWarnInterval) == std::cv_status::timeout);
        }
    }

    LOG(INFO) << "All workers started";
    started_ = true;
    watchdog_timer_id_ = startTimer(Seconds(5));
}


//--------------------------------------------------------------------------------------------------
void WorkerManager::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != watchdog_timer_id_)
        return;

    const TimePoint now = Clock::now();
    QStringList stalled;

    for (const auto& entry : workers_)
    {
        Worker* worker = entry.second.get();

        if (worker->pong_pending_.load(std::memory_order_relaxed))
        {
            const Seconds stall_time = DurationCast<Seconds>(now - worker->ping_time_);
            stalled.append(QString("%1 (%2 s)").arg(worker->name()).arg(stall_time.count()));
            worker->stall_reported_ = true;
            continue;
        }

        if (worker->stall_reported_)
        {
            worker->stall_reported_ = false;
            LOG(WARNING) << "Worker" << worker->name() << "event loop recovered";
        }

        worker->ping_time_ = now;
        worker->pong_pending_.store(true, std::memory_order_relaxed);
        worker->post([worker]()
        {
            worker->pong_pending_.store(false, std::memory_order_relaxed);
        });
    }

    if (!stalled.isEmpty())
        LOG(ERROR) << "Worker event loops are stalled:" << stalled.join(", ");
}
