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

#include <QTimerEvent>

#include "base/threading/worker_manager.h"

//--------------------------------------------------------------------------------------------------
// static
thread_local Worker* Worker::current_worker_ = nullptr;

//--------------------------------------------------------------------------------------------------
Worker::Worker(Thread::EventDispatcher dispatcher, Milliseconds timer_interval)
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
void Worker::post(std::function<void()> work)
{
    QMetaObject::invokeMethod(this, std::move(work), Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void Worker::start(WorkerManager* manager)
{
    manager_ = manager;
    thread_.start();
}

//--------------------------------------------------------------------------------------------------
void Worker::stopSoon()
{
    thread_.quit();
}

//--------------------------------------------------------------------------------------------------
void Worker::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == timer_id_)
        onTimer();
}

//--------------------------------------------------------------------------------------------------
void Worker::onThreadStarted()
{
    current_worker_ = this;

    onStart();

    if (timer_interval_ > Milliseconds::zero())
        timer_id_ = startTimer(timer_interval_);

    std::lock_guard lock(manager_->lock_);
    ++manager_->running_;
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

    std::lock_guard lock(manager_->lock_);
    --manager_->running_;
    manager_->condition_.notify_all();
}
