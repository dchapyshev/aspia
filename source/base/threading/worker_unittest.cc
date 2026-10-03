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
    std::atomic<void*> prepare_thread_id{ nullptr };
    std::atomic<bool> signal_received{ false };
    std::atomic<bool> sibling_connected_at_stop{ false };
    std::atomic<bool> self_connected_at_stop{ false };

    // Written in onStart(); reading from the test thread is ordered by the start() barrier.
    QString thread_name;

    std::function<void()> on_prepare;
    std::function<void()> on_start;
    std::function<void()> on_stop;
};

class TestWorkerB;

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
    void onPrepare() final;

    void onStart() final
    {
        state_->start_thread_id = QThread::currentThreadId();
        state_->thread_name = QThread::currentThread()->objectName();
        if (state_->on_start)
            state_->on_start();
        state_->started = true;
    }

    void onStop() final;

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

signals:
    void sig_started();

protected:
    void onStart() final
    {
        state_->start_thread_id = QThread::currentThreadId();
        state_->sibling_found = (findWorker<TestWorkerA>() != nullptr);
        state_->started = true;
        emit sig_started();
    }

    void onStop() final { state_->stopped = true; }

private:
    std::shared_ptr<WorkerTestState> state_;
};

//--------------------------------------------------------------------------------------------------
void TestWorkerA::onPrepare()
{
    state_->prepare_thread_id = QThread::currentThreadId();

    if (state_->on_prepare)
        state_->on_prepare();

    if (TestWorkerB* sibling = findWorker<TestWorkerB>())
    {
        connect(sibling, &TestWorkerB::sig_started, this,
                [this]() { state_->signal_received = true; }, Qt::QueuedConnection);
    }

    connect(this, &Worker::sig_tick, this, [](TimePoint /* now */) {});
}

//--------------------------------------------------------------------------------------------------
void TestWorkerA::onStop()
{
    state_->stop_thread_id = QThread::currentThreadId();

    // A disconnect succeeds only for a connection that still exists.
    if (TestWorkerB* sibling = findWorker<TestWorkerB>())
        state_->sibling_connected_at_stop = disconnect(sibling, &TestWorkerB::sig_started, this, nullptr);
    state_->self_connected_at_stop = disconnect(this, &Worker::sig_tick, this, nullptr);

    if (state_->on_stop)
        state_->on_stop();
    state_->stopped = true;
}

namespace {

const MilliSeconds kWaitTimeout{ 5000 };

//--------------------------------------------------------------------------------------------------
bool waitFor(const std::function<bool()>& condition)
{
    const TimePoint deadline = Clock::now() + kWaitTimeout;

    while (!condition())
    {
        if (Clock::now() >= deadline)
            return false;

        std::this_thread::sleep_for(MilliSeconds(10));
    }

    return true;
}

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

// A subscription made in onPrepare() catches a signal the sibling emits from onStart(), no matter
// which thread is ahead. The subscriber dawdles in onPrepare(), so without the phasing the sibling
// would announce its start before the subscription exists.
TEST(WorkerTests, SignalFromOnStartReachesSubscriptionFromOnPrepare)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();
    state_a->on_prepare = []() { std::this_thread::sleep_for(MilliSeconds(100)); };

    WorkerManager manager;
    manager.add(std::make_unique<TestWorkerA>(state_a));
    manager.add(std::make_unique<TestWorkerB>(state_b));
    manager.start();

    EXPECT_EQ(state_a->prepare_thread_id, state_a->start_thread_id);

    // The signal is queued to the thread of the subscriber, which runs its loop by now.
    const TimePoint wait_start = Clock::now();
    while (!state_a->signal_received && Clock::now() - wait_start < Seconds(5))
        std::this_thread::sleep_for(MilliSeconds(10));

    EXPECT_TRUE(state_a->signal_received);
}

// A worker that is being stopped no longer receives signals from its siblings, while its connections
// to itself stay.
TEST(WorkerTests, DestructorDisconnectsWorkersBeforeStopping)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state_a));
        manager.add(std::make_unique<TestWorkerB>(state_b));
        manager.start();
    }

    EXPECT_TRUE(state_a->stopped);
    EXPECT_FALSE(state_a->sibling_connected_at_stop);
    EXPECT_TRUE(state_a->self_connected_at_stop);
}

// The work posted to a worker whose thread has finished is dropped at once: the finishing thread
// deletes its event dispatcher, so nothing may be queued to it any more.
TEST(WorkerTests, PostAfterThreadFinishedIsDropped)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    std::atomic<bool> target_finished{ false };
    std::atomic<bool> work_released{ false };
    std::atomic<bool> work_executed{ false };

    {
        WorkerManager manager;
        manager.add(std::make_unique<TestWorkerA>(state_a));
        const qint64 target_id = manager.add(std::make_unique<TestWorkerA>(state_b));

        TestWorkerA* target = manager.find<TestWorkerA>(target_id);
        ASSERT_TRUE(target);

        // Both workers stop together; this one posts once the thread of the other has finished.
        state_a->on_stop = [&, target]()
        {
            target_finished = waitFor([target]() { return target->thread()->isFinished(); });

            auto marker = std::make_shared<int>(0);
            std::weak_ptr<int> weak_marker = marker;

            target->post([marker = std::move(marker), &work_executed]() { work_executed = true; });
            work_released = weak_marker.expired();
        };

        manager.start();
    }

    EXPECT_TRUE(target_finished);
    EXPECT_TRUE(work_released);
    EXPECT_FALSE(work_executed);
}

// The reply to a request is dropped at once as well when the thread of the caller has finished
// before the request is done.
TEST(WorkerTests, RequestReplyToFinishedCallerIsDropped)
{
    auto state_a = std::make_shared<WorkerTestState>();
    auto state_b = std::make_shared<WorkerTestState>();

    std::atomic<bool> request_started{ false };
    std::atomic<bool> caller_finished{ false };
    std::atomic<bool> reply_released{ false };
    std::atomic<bool> reply_executed{ false };

    // Written in the caller's onStart(), read in the target's onStop(); ordered by the start() barrier.
    std::weak_ptr<int> weak_marker;

    {
        WorkerManager manager;
        const qint64 caller_id = manager.add(std::make_unique<TestWorkerA>(state_a));
        const qint64 target_id = manager.add(std::make_unique<TestWorkerA>(state_b));

        TestWorkerA* caller = manager.find<TestWorkerA>(caller_id);
        TestWorkerA* target = manager.find<TestWorkerA>(target_id);
        ASSERT_TRUE(caller);
        ASSERT_TRUE(target);

        // The request is done only after the caller has stopped and its thread has finished.
        state_a->on_start = [&, caller, target]()
        {
            auto marker = std::make_shared<int>(0);
            weak_marker = marker;

            target->request(caller,
                [&, caller]() -> int
                {
                    request_started = true;
                    caller_finished = waitFor([caller]() { return caller->thread()->isFinished(); });
                    return 0;
                },
                [marker, &reply_executed](int /* value */) { reply_executed = true; });
        };

        // The caller is not destroyed yet, so a reply still queued to it would keep the marker.
        state_b->on_stop = [&]() { reply_released = weak_marker.expired(); };

        manager.start();

        // A stop that comes before the target runs the request drops the request unexecuted.
        ASSERT_TRUE(waitFor([&]() { return request_started.load(); }));
    }

    EXPECT_TRUE(caller_finished);
    EXPECT_TRUE(reply_released);
    EXPECT_FALSE(reply_executed);
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
