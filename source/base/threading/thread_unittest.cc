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

#include "base/threading/thread.h"

#include <QAbstractEventDispatcher>
#include <QPointer>
#include <QSemaphore>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "base/time_types.h"
#include "base/threading/asio_event_dispatcher.h"

#if defined(Q_OS_WINDOWS)
#include <objbase.h>
#endif

namespace {

const MilliSeconds kWaitTimeout{ 5000 };

} // namespace

TEST(ThreadTests, SignalsEmittedAroundEventLoopInThread)
{
    std::atomic<void*> before_thread_id{ nullptr };
    std::atomic<void*> after_thread_id{ nullptr };
    std::atomic<int> sequence{ 0 };
    std::atomic<int> before_order{ 0 };
    std::atomic<int> after_order{ 0 };
    QSemaphore started;

    Thread thread(Thread::AsioDispatcher);

    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        before_thread_id = QThread::currentThreadId();
        before_order = ++sequence;
        started.release();
    });
    QObject::connect(&thread, &Thread::sig_afterRunning, [&]()
    {
        after_thread_id = QThread::currentThreadId();
        after_order = ++sequence;
    });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    // Both signals are emitted from the new thread, strictly around the event loop.
    EXPECT_NE(before_thread_id, QThread::currentThreadId());
    EXPECT_EQ(before_thread_id, after_thread_id);
    EXPECT_EQ(before_order, 1);
    EXPECT_EQ(after_order, 2);
    EXPECT_TRUE(thread.isFinished());
}

TEST(ThreadTests, QueuedInvokesRunInThread)
{
    std::atomic<void*> thread_id{ nullptr };
    std::atomic<void*> invoke_thread_id{ nullptr };
    QSemaphore started;
    QSemaphore done;

    Thread thread(Thread::AsioDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        thread_id = QThread::currentThreadId();
        started.release();
    });

    QObject context;
    context.moveToThread(&thread);

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));

    QMetaObject::invokeMethod(&context, [&]()
    {
        invoke_thread_id = QThread::currentThreadId();
        done.release();
    },
    Qt::QueuedConnection);

    ASSERT_TRUE(done.tryAcquire(1, kWaitTimeout));
    EXPECT_EQ(invoke_thread_id, thread_id);

    thread.stop();
}

TEST(ThreadTests, StopIsIdempotentAndSafeWithoutStart)
{
    {
        Thread thread(Thread::AsioDispatcher);
        thread.stop();
        thread.stop();
    }

    {
        QSemaphore started;
        Thread thread(Thread::AsioDispatcher);
        QObject::connect(&thread, &Thread::sig_beforeRunning, [&]() { started.release(); });

        thread.start();
        ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));

        thread.stop();
        EXPECT_TRUE(thread.isFinished());
        thread.stop();
    }
}

TEST(ThreadTests, DestructorStopsRunningThread)
{
    std::atomic<bool> finished{ false };
    QSemaphore started;

    {
        Thread thread(Thread::AsioDispatcher);
        QObject::connect(&thread, &Thread::sig_beforeRunning, [&]() { started.release(); });
        QObject::connect(&thread, &Thread::sig_afterRunning, [&]() { finished = true; });

        thread.start();
        ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    }

    EXPECT_TRUE(finished);
}

TEST(ThreadTests, AsioDispatcherIsInstalled)
{
    std::atomic<bool> is_asio{ false };
    QSemaphore started;

    Thread thread(Thread::AsioDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        is_asio = (qobject_cast<AsioEventDispatcher*>(QAbstractEventDispatcher::instance()) != nullptr);
        started.release();
    });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    EXPECT_TRUE(is_asio);
}

TEST(ThreadTests, QtDispatcherIsInstalled)
{
    std::atomic<bool> is_asio{ true };
    QSemaphore started;

    Thread thread(Thread::QtDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        is_asio = (qobject_cast<AsioEventDispatcher*>(QAbstractEventDispatcher::instance()) != nullptr);
        started.release();
    });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    EXPECT_FALSE(is_asio);
}

TEST(ThreadTests, RestartAfterStop)
{
    std::atomic<int> before_count{ 0 };
    std::atomic<int> after_count{ 0 };
    QSemaphore started;

    Thread thread(Thread::AsioDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        ++before_count;
        started.release();
    });
    QObject::connect(&thread, &Thread::sig_afterRunning, [&]() { ++after_count; });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();
    EXPECT_EQ(before_count, 1);
    EXPECT_EQ(after_count, 1);

    // QThread is restartable; the run() wrapper (signals, dispatcher, COM) must survive a
    // second cycle.
    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();
    EXPECT_EQ(before_count, 2);
    EXPECT_EQ(after_count, 2);
    EXPECT_TRUE(thread.isFinished());
}

TEST(ThreadTests, QueuedInvokePostedBeforeStartRunsAfterStart)
{
    std::atomic<void*> invoke_thread_id{ nullptr };
    QSemaphore done;

    Thread thread(Thread::AsioDispatcher);

    QObject context;
    context.moveToThread(&thread);

    // Posted while the thread is not running yet; must be delivered once the loop starts.
    QMetaObject::invokeMethod(&context, [&]()
    {
        invoke_thread_id = QThread::currentThreadId();
        done.release();
    },
    Qt::QueuedConnection);

    thread.start();
    ASSERT_TRUE(done.tryAcquire(1, kWaitTimeout));
    EXPECT_NE(invoke_thread_id, QThread::currentThreadId());

    thread.stop();
}

TEST(ThreadTests, DeferredDeleteProcessedByRunningLoop)
{
    QSemaphore destroyed;

    Thread thread(Thread::AsioDispatcher);

    QPointer<QObject> guard(new QObject());
    guard->moveToThread(&thread);
    QObject::connect(guard.data(), &QObject::destroyed, [&]() { destroyed.release(); });

    thread.start();

    QObject* object = guard.data();
    QMetaObject::invokeMethod(object, [object]() { object->deleteLater(); }, Qt::QueuedConnection);

    // The running loop itself must pick the deferred delete up, well before the thread stops.
    ASSERT_TRUE(destroyed.tryAcquire(1, kWaitTimeout));

    // QPointer is cleared slightly after destroyed() is emitted; give it a moment.
    for (int i = 0; i < 500 && !guard.isNull(); ++i)
        std::this_thread::sleep_for(MilliSeconds(10));
    EXPECT_TRUE(guard.isNull());

    thread.stop();
}

#if defined(Q_OS_WINDOWS)
TEST(ThreadTests, AsioDispatcherThreadJoinsComMta)
{
    std::atomic<APTTYPE> apt_type{ APTTYPE_CURRENT };
    QSemaphore started;

    Thread thread(Thread::AsioDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        APTTYPE type;
        APTTYPEQUALIFIER qualifier;
        if (SUCCEEDED(CoGetApartmentType(&type, &qualifier)))
            apt_type = type;
        started.release();
    });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    // Asio threads join the COM multithreaded apartment (ScopedCOMInitializer::kMTA in run()).
    EXPECT_EQ(apt_type, APTTYPE_MTA);
}

TEST(ThreadTests, QtDispatcherThreadIsComSta)
{
    std::atomic<APTTYPE> apt_type{ APTTYPE_CURRENT };
    QSemaphore started;

    Thread thread(Thread::QtDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]()
    {
        APTTYPE type;
        APTTYPEQUALIFIER qualifier;
        if (SUCCEEDED(CoGetApartmentType(&type, &qualifier)))
            apt_type = type;
        started.release();
    });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    // Non-asio threads get the default single-threaded apartment. The first STA thread in the
    // process is reported as the main STA.
    EXPECT_TRUE(apt_type == APTTYPE_STA || apt_type == APTTYPE_MAINSTA);
}
#endif // defined(Q_OS_WINDOWS)

TEST(ThreadTests, DeferredDeleteFlushedAfterEventLoopExit)
{
    QSemaphore started;

    Thread thread(Thread::AsioDispatcher);
    QObject::connect(&thread, &Thread::sig_beforeRunning, [&]() { started.release(); });

    // The object lives in the new thread; deleteLater() is issued after exec() has already
    // returned (the same stage where Worker::onStop() runs), so only the deferred-delete flush
    // at the end of the thread can pick it up. Worker teardown relies on this contract.
    QPointer<QObject> guard(new QObject());
    guard->moveToThread(&thread);

    QObject* object = guard.data();
    QObject::connect(&thread, &Thread::sig_afterRunning, [object]() { object->deleteLater(); });

    thread.start();
    ASSERT_TRUE(started.tryAcquire(1, kWaitTimeout));
    thread.stop();

    EXPECT_TRUE(guard.isNull());
}
