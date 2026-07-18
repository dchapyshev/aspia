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

#ifndef BASE_THREADING_WORKER_H
#define BASE_THREADING_WORKER_H

#include <QObject>

#include <chrono>

#include "base/threading/thread.h"

class QTimer;
class WorkerManager;

class Worker : public QObject
{
    Q_OBJECT

public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;

    // If timer_interval is non-zero, the worker gets a repeating timer that fires onTimer() from its
    // own thread at that interval; the timer lives and dies with the worker thread.
    explicit Worker(Thread::EventDispatcher dispatcher = Thread::AsioDispatcher,
                    Milliseconds timer_interval = Milliseconds::zero());
    ~Worker() override;

protected:
    friend class WorkerManager;

    void start(WorkerManager* manager);
    void stopSoon();

    // Finds a sibling worker registered in the same manager. Safe to call from the worker thread
    // once the worker has been started (the worker set does not change after that). Defined in
    // worker_manager.h.
    template <typename T>
    T* findWorker() const;

    // Runs inside the worker thread before its event loop starts. Create here every
    // object that must live in the worker thread.
    virtual void onStart() = 0;

    // Runs inside the worker thread after its event loop ends. Destroy here everything
    // created in onStart().
    virtual void onStop() = 0;

    // Called on the worker thread at the interval passed to the constructor (only if it was
    // non-zero). The default does nothing.
    virtual void onTimer() { /* Nothing */ }

private slots:
    void onThreadStarted();
    void onThreadFinished();

private:
    WorkerManager* manager_ = nullptr;
    Thread thread_;

    const Milliseconds timer_interval_;
    QTimer* timer_ = nullptr;

    Q_DISABLE_COPY_MOVE(Worker)
};

#endif // BASE_THREADING_WORKER_H
