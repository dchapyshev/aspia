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

#include <QSemaphore>
#include <QThread>

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

// Observable side effects of a test worker. Held by shared_ptr so the state outlives the worker,
// which is destroyed by the manager.
struct WorkerTestState
{
    std::atomic<bool> started{ false };
    std::atomic<bool> stopped{ false };
    std::atomic<int> ticks{ 0 };
    std::atomic<void*> start_thread_id{ nullptr };
    std::atomic<void*> stop_thread_id{ nullptr };
    std::atomic<bool> sibling_found{ false };

    // Written in onStart(); reading from the test thread is ordered by the start() barrier.
    QString thread_name;

    std::function<void()> on_start;
    std::function<void()> on_stop;
};

// Q_OBJECT classes cannot live in an anonymous namespace (moc limitation).
class TestWorkerA final : public Worker
{
    Q_OBJECT

public:
    explicit TestWorkerA(std::shared_ptr<WorkerTestState> state,
                         MilliSeconds timer_interval = MilliSeconds::zero())
        : Worker(Thread::AsioDispatcher, timer_interval),
          state_(std::move(state))
    {
        // Nothing
    }

protected:
    void onStart() final
    {
        state_->start_thread_id = QThread::currentThreadId();
        state_->thread_name = QThread::currentThread()->objectName();
        if (state_->on_start)
            state_->on_start();
        state_->started = true;
    }

    void onStop() final
    {
        state_->stop_thread_id = QThread::currentThreadId();
        if (state_->on_stop)
            state_->on_stop();
        state_->stopped = true;
    }

    void onTimer(TimePoint /* now */) final { ++state_->ticks; }

private:
    std::shared_ptr<WorkerTestState> state_;
};

class TestWorkerB final : public Worker
{
    Q_OBJECT

public:
    explicit TestWorkerB(std::shared_ptr<WorkerTestState> state)
        : state_(std::move(state))
    {
        // Nothing
    }

protected:
    void onStart() final
    {
        state_->start_thread_id = QThread::currentThreadId();
        state_->sibling_found = (findWorker<TestWorkerA>() != nullptr);
        state_->started = true;
    }

    void onStop() final { state_->stopped = true; }

private:
    std::shared_ptr<WorkerTestState> state_;
};

namespace {

const MilliSeconds kWaitTimeout{ 5000 };

} // namespace

TEST(WorkerTests, StartRunsEveryOnStartBeforeReturn)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state_a));
    manager.add(std::make_unique<TestWorkerB>(state_b));
    manager.start();

    // start() must not return until every onStart() has completed, each in its own thread.
    EXPECT_TRUE(state_a->started);
    EXPECT_TRUE(state_b->started);
    EXPECT_NE(state_a->start_thread_id, QThread::currentThreadId());
    EXPECT_NE(state_b->start_thread_id, QThread::currentThreadId());
    EXPECT_NE(state_a->start_thread_id, state_b->start_thread_id);
}

TEST(WorkerTests, StartWaitsForSlowOnStart)
{
    auto state = std::make_shared<WorkerTestState>();
    state->on_start = []() { std::this_thread::sleep_for(MilliSeconds(300)); };

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state));

    const TimePoint before = Clock::now();
    manager.start();
    const MilliSeconds elapsed = DurationCast<MilliSeconds>(Clock::now() - before);

    EXPECT_TRUE(state->started);
    EXPECT_GE(elapsed, MilliSeconds(250));
}

TEST(WorkerTests, DestructorCallsOnStopInWorkerThread)
{
    auto state = std::make_shared<WorkerTestState>();

    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state));
        manager.start();
    }

    EXPECT_TRUE(state->stopped);
    EXPECT_NE(state->stop_thread_id, QThread::currentThreadId());
    EXPECT_EQ(state->stop_thread_id, state->start_thread_id);
}

TEST(WorkerTests, DestructorWaitsForSlowOnStop)
{
    auto state = std::make_shared<WorkerTestState>();
    state->on_stop = []() { std::this_thread::sleep_for(MilliSeconds(300)); };

    TimePoint before;
    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state));
        manager.start();
        before = Clock::now();
    }
    const MilliSeconds elapsed = DurationCast<MilliSeconds>(Clock::now() - before);

    EXPECT_TRUE(state->stopped);
    EXPECT_GE(elapsed, MilliSeconds(250));
}

TEST(WorkerTests, PostExecutesInWorkerThread)
{
    auto state = std::make_shared<WorkerTestState>();

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state));
    manager.start();

    TestWorkerA* worker = manager.find<TestWorkerA>();
    ASSERT_TRUE(worker);

    QSemaphore done;
    std::atomic<void*> post_thread_id{ nullptr };
    std::atomic<Worker*> current{ nullptr };

    worker->post([&]()
    {
        post_thread_id = QThread::currentThreadId();
        current = Worker::current();
        done.release();
    });

    ASSERT_TRUE(done.tryAcquire(1, kWaitTimeout));
    EXPECT_EQ(post_thread_id, state->start_thread_id);
    EXPECT_EQ(current, worker);
    EXPECT_EQ(Worker::current(), nullptr);
}

TEST(WorkerTests, TimerTicksUntilStopped)
{
    auto state = std::make_shared<WorkerTestState>();

    int ticks_after = 0;
    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state, MilliSeconds(10)));
        manager.start();

        while (state->ticks < 3)
            std::this_thread::sleep_for(MilliSeconds(10));
    }

    // The timer dies with the worker thread; the counter must not advance anymore.
    ticks_after = state->ticks;
    std::this_thread::sleep_for(MilliSeconds(100));
    EXPECT_EQ(state->ticks, ticks_after);
}

TEST(WorkerTests, NameIsDerivedClassName)
{
    auto state = std::make_shared<WorkerTestState>();

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state));
    manager.start();

    TestWorkerA* worker = manager.find<TestWorkerA>();
    ASSERT_TRUE(worker);

    EXPECT_EQ(worker->name(), "TestWorkerA");
    // The OS thread is named after the worker (set through the thread object name).
    EXPECT_EQ(state->thread_name, "TestWorkerA");
}

TEST(WorkerTests, FindWorkerFindsSibling)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state_a));
    manager.add(std::make_unique<TestWorkerB>(state_b));
    manager.start();

    EXPECT_TRUE(state_b->sibling_found);
}

TEST(WorkerTests, RequestDeliversReplyToCallerThread)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state_a));
    manager.add(std::make_unique<TestWorkerB>(state_b));
    manager.start();

    TestWorkerA* caller = manager.find<TestWorkerA>();
    TestWorkerB* target = manager.find<TestWorkerB>();
    ASSERT_TRUE(caller);
    ASSERT_TRUE(target);

    QSemaphore done;
    std::atomic<void*> request_thread_id{ nullptr };
    std::atomic<void*> reply_thread_id{ nullptr };
    std::atomic<int> reply_value{ 0 };

    // request() must be called from a worker thread; the request runs in the target worker and
    // the reply comes back to the caller's thread.
    caller->post([&, caller, target]()
    {
        target->request(caller,
            [&]() -> int
            {
                request_thread_id = QThread::currentThreadId();
                return 42;
            },
            [&](int value)
            {
                reply_thread_id = QThread::currentThreadId();
                reply_value = value;
                done.release();
            });
    });

    ASSERT_TRUE(done.tryAcquire(1, kWaitTimeout));
    EXPECT_EQ(request_thread_id, state_b->start_thread_id);
    EXPECT_EQ(reply_thread_id, state_a->start_thread_id);
    EXPECT_EQ(reply_value, 42);
}

TEST(WorkerTests, DestructorWithoutStartDoesNotHang)
{
    auto state = std::make_shared<WorkerTestState>();

    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state));
    }

    EXPECT_FALSE(state->started);
    EXPECT_FALSE(state->stopped);
}

TEST(WorkerTests, EmptyManagerStartsAndStops)
{
    WorkerManager manager;
    manager.start();
}

#include "worker_unittest.moc"
