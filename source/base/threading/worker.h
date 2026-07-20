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
#include <QPointer>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "base/logging.h"
#include "base/threading/thread.h"

class QTimerEvent;
class WorkerManager;

class Worker : public QObject
{
    Q_OBJECT

public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    using Hours = std::chrono::hours;

    // If timer_interval is non-zero, the worker gets a repeating timer that fires onTimer() from its
    // own thread at that interval; the timer lives and dies with the worker thread.
    explicit Worker(Thread::EventDispatcher dispatcher = Thread::AsioDispatcher,
                    Milliseconds timer_interval = Milliseconds::zero());
    ~Worker() override;

    // Returns the worker owning the calling thread, or nullptr if the thread is not a worker.
    static Worker* current() { return current_worker_; }

    // Executes |work| asynchronously in the worker thread. May be called from any thread.
    void post(std::function<void()> work);

    // Request-response: executes |request| in the worker thread and delivers its return value to
    // the calling thread through |reply|. The reply is dropped if |context| is destroyed before
    // it arrives. |request| must return a value (use post() for fire-and-forget work). Must be
    // called from a worker thread.
    template <typename Request, typename Reply>
    void request(QObject* context, Request request, Reply reply)
    {
        // The reply is never posted to |context| directly: an object owned by another thread can
        // die at any moment, making the post a use-after-free.
        Worker* caller = current_worker_;
        CHECK(caller);

        QPointer<QObject> ctx(context);
        post([caller, ctx, request = std::move(request), reply = std::move(reply)]() mutable
        {
            QMetaObject::invokeMethod(caller,
                [ctx, reply = std::move(reply), result = request()]() mutable
            {
                if (ctx)
                    reply(std::move(result));
            },
            Qt::QueuedConnection);
        });
    }

signals:
    // Per-thread clock: emitted from the worker thread on every built-in timer tick.
    void sig_tick(const TimePoint& now);

protected:
    friend class WorkerManager;

    void start(WorkerManager* manager);
    void stopSoon();

    // Finds a sibling worker registered in the same manager. Safe to call from the worker thread
    // once the worker has been started (the worker set does not change after that). Defined below
    // the WorkerManager class.
    template <typename T>
    T* findWorker() const;

    // Runs inside the worker thread before its event loop starts. Create here every
    // object that must live in the worker thread.
    virtual void onStart() = 0;

    // Runs inside the worker thread after its event loop ends. Destroy here everything
    // created in onStart().
    virtual void onStop() = 0;

    // Called on the worker thread at the interval passed to the constructor (only if it was
    // non-zero). |now| is the current tick timepoint, shared with sig_tick(). The default does
    // nothing.
    virtual void onTimer(const TimePoint& /* now */) { /* Nothing */ }

    // QObject implementation.
    void timerEvent(QTimerEvent* event) final;

private slots:
    void onThreadStarted();
    void onThreadFinished();

private:
    WorkerManager* manager_ = nullptr;
    Thread thread_;

    const Milliseconds timer_interval_;
    int timer_id_ = 0;

    static thread_local Worker* current_worker_;

    Q_DISABLE_COPY_MOVE(Worker)
};

class WorkerManager
{
public:
    WorkerManager();
    ~WorkerManager();

    qint64 add(std::unique_ptr<Worker> worker);
    void start();

    template <typename T>
    T* find(qint64 worker_id)
    {
        auto it = workers_.find(worker_id);
        if (it == workers_.end())
            return nullptr;

        return dynamic_cast<T*>(it->second.get());
    }

    // Finds a worker by its type. Safe to call from any worker thread once the manager has started
    // (the worker set does not change afterwards).
    template <typename T>
    T* find()
    {
        for (const auto& worker : workers_)
        {
            if (T* result = dynamic_cast<T*>(worker.second.get()))
                return result;
        }

        return nullptr;
    }

private:
    friend class Worker;

    const std::thread::id thread_id_;

    std::condition_variable condition_;
    std::mutex lock_;
    size_t running_ = 0;
    bool started_ = false;
    qint64 next_worker_id_ = 0;

    std::unordered_map<qint64, std::unique_ptr<Worker>> workers_;

    Q_DISABLE_COPY_MOVE(WorkerManager)
};

// Defined here because it needs the complete WorkerManager type.
template <typename T>
T* Worker::findWorker() const
{
    CHECK(manager_);
    return manager_->find<T>();
}

#endif // BASE_THREADING_WORKER_H
