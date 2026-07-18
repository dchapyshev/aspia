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

#include <QTimer>

#include "base/threading/worker_manager.h"

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
void Worker::onThreadStarted()
{
    onStart();

    if (timer_interval_ > Milliseconds::zero())
    {
        timer_ = new QTimer(this);
        timer_->setInterval(timer_interval_);
        connect(timer_, &QTimer::timeout, this, &Worker::onTimer);
        timer_->start();
    }

    std::lock_guard lock(manager_->lock_);
    ++manager_->running_;
    manager_->condition_.notify_all();
}

//--------------------------------------------------------------------------------------------------
void Worker::onThreadFinished()
{
    delete timer_;
    timer_ = nullptr;

    onStop();

    std::lock_guard lock(manager_->lock_);
    --manager_->running_;
    manager_->condition_.notify_all();
}
