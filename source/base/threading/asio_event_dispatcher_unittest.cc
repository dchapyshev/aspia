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

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QHostInfo>
#include <QSemaphore>
#include <QSocketNotifier>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#if defined(Q_OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <limits>

#include "base/time_types.h"
#include "base/threading/thread.h"

namespace {

#if defined(Q_OS_WINDOWS)
using NativeSocket = SOCKET;
const NativeSocket kInvalidNativeSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
const NativeSocket kInvalidNativeSocket = -1;
#endif

// Creates a non-blocking UDP socket bound to the loopback interface. Raw sockets are used
// instead of QAbstractSocket because the latter registers its own notifiers in the dispatcher
// and would occupy the per-type slots these tests need to control manually. UDP gives exact
// control over readability: one datagram - one readiness event.
NativeSocket createBoundUdpSocket(quint16* port)
{
#if defined(Q_OS_WINDOWS)
    // Sockets are created directly instead of through QtNetwork, so winsock is initialized
    // manually.
    static const int winsock_result = []()
    {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (winsock_result != 0)
        return kInvalidNativeSocket;
#endif

    NativeSocket sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == kInvalidNativeSocket)
        return kInvalidNativeSocket;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        return kInvalidNativeSocket;

    socklen_t addr_size = sizeof(addr);
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &addr_size) != 0)
        return kInvalidNativeSocket;

    *port = ntohs(addr.sin_port);

#if defined(Q_OS_WINDOWS)
    u_long non_blocking = 1;
    ioctlsocket(sock, FIONBIO, &non_blocking);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif

    return sock;
}

void sendDatagram(NativeSocket from, quint16 to_port)
{
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(to_port);

    const char payload = 'x';
    ::sendto(from, &payload, sizeof(payload), 0,
             reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

void drainSocket(NativeSocket sock)
{
    char buffer[64];
    while (::recv(sock, buffer, sizeof(buffer), 0) > 0);
}

void closeNativeSocket(NativeSocket sock)
{
#if defined(Q_OS_WINDOWS)
    closesocket(sock);
#else
    ::close(sock);
#endif
}

qint64 msSince(TimePoint since)
{
    return DurationCast<Milliseconds>(Clock::now() - since).count();
}

void pumpFor(int duration_ms)
{
    const TimePoint start_time = Clock::now();
    while (msSince(start_time) < duration_ms)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

template <typename Predicate>
bool pumpUntil(Predicate condition, int timeout_ms)
{
    const TimePoint start_time = Clock::now();
    while (!condition() && msSince(start_time) < timeout_ms)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

} // namespace

class ED_TestObject : public QObject
{
public:
    static int customType()
    {
        static int t = QEvent::registerEventType();
        return t;
    }

    int received = 0;
    QVector<qint64> deltas;
    qint64 lastMs = -1;
    TimePoint clk;

protected:
    void customEvent(QEvent* ev) override
    {
        if (ev->type() == customType())
        {
            ++received;
            qint64 now = msSince(clk);
            if (lastMs >= 0) deltas.append(now - lastMs);
            lastMs = now;
        }
        QObject::customEvent(ev);
    }
};

TEST(DispatcherTests, PostEvent_DeliversAll)
{
    // Preconditions
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    ED_TestObject obj;
    obj.moveToThread(QThread::currentThread());
    obj.clk = Clock::now();

    // Post a bunch of custom events
    constexpr int N = 64;
    for (int i = 0; i < N; ++i)
    {
        QCoreApplication::postEvent(&obj,
            new QEvent(static_cast<QEvent::Type>(ED_TestObject::customType())));
    }

    // Pump the loop until all are processed or timeout
    const TimePoint wait_start = Clock::now();
    while (obj.received < N && msSince(wait_start) < 2000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }

    EXPECT_EQ(obj.received, N);
}

// Cross-thread wakeups
TEST(DispatcherTests, CrossThread_Post_WakeupLatency_Soft)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    ED_TestObject obj;
    obj.clk = Clock::now();

    constexpr int N = 24;
    constexpr int gapMs = 5;

    std::atomic<int> posted{0};
    std::thread worker([&]()
    {
        for (int i = 0; i < N; ++i)
        {
            QCoreApplication::postEvent(&obj,
                new QEvent(static_cast<QEvent::Type>(ED_TestObject::customType())));
            ++posted;
            QThread::msleep(gapMs);
        }
    });

    const TimePoint wait_start = Clock::now();
    while ((obj.received < N || posted.load() < N) && msSince(wait_start) < 4000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    worker.join();

    ASSERT_EQ(posted.load(), N);
    ASSERT_EQ(obj.received, N);

    if (!obj.deltas.isEmpty())
    {
        const double avg = std::accumulate(obj.deltas.begin(), obj.deltas.end(), 0.0) / obj.deltas.size();
        EXPECT_LT(avg, 50.0);
    }
}

// An idle event loop parks in a blocking wait inside processEvents. Unlike the latency test
// above, the receiving loop here is not pumped manually, so a queued call must get through the
// wakeUp() path to abort the blocking wait. A coalesced-away or lost wakeup would stall the
// iteration until the acquire timeout.
TEST(DispatcherTests, CrossThread_Post_WakesBlockedLoop)
{
    Thread thread(Thread::AsioDispatcher);
    thread.start();

    // The probe lives in the worker thread, queued calls to it run there.
    QObject probe;
    probe.moveToThread(&thread);

    QSemaphore processed;
    QMetaObject::invokeMethod(&probe, [&]() { processed.release(); }, Qt::QueuedConnection);
    ASSERT_TRUE(processed.tryAcquire(1, 5000));

    for (int i = 0; i < 20; ++i)
    {
        // Give the loop time to park in the blocking wait again.
        QThread::msleep(10);

        const TimePoint latency_start = Clock::now();

        QMetaObject::invokeMethod(&probe, [&]() { processed.release(); }, Qt::QueuedConnection);
        ASSERT_TRUE(processed.tryAcquire(1, 5000));
        EXPECT_LT(msSince(latency_start), 1000);
    }

    thread.stop();
}

// QThread::quit() reaches the dispatcher through interrupt(). It must abort the blocking wait
// immediately instead of waiting for the next timer or socket event.
TEST(DispatcherTests, CrossThread_Quit_WakesBlockedLoop)
{
    Thread thread(Thread::AsioDispatcher);
    thread.start();

    QObject probe;
    probe.moveToThread(&thread);

    // Make sure the worker event loop is actually running before quitting it.
    QSemaphore started;
    QMetaObject::invokeMethod(&probe, [&]() { started.release(); }, Qt::QueuedConnection);
    ASSERT_TRUE(started.tryAcquire(1, 5000));

    // Let the loop park in the blocking wait with no pending work.
    QThread::msleep(100);

    const TimePoint latency_start = Clock::now();

    thread.quit();
    ASSERT_TRUE(thread.wait(5000));
    EXPECT_LT(msSince(latency_start), 1000);
}

// Nested event loops
TEST(DispatcherTests, NestedEventLoops_NoDeadlock)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    bool entered = false;
    bool finished = false;

    QTimer::singleShot(Milliseconds(5), [&]()
    {
        entered = true;
        QEventLoop inner;
        QTimer::singleShot(Milliseconds(30), &inner, [&](){ inner.quit(); });
        inner.exec();
        finished = true;
    });

    const TimePoint wait_start = Clock::now();
    while (!finished && msSince(wait_start) < 2000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }

    EXPECT_TRUE(entered);
    EXPECT_TRUE(finished);
}

TEST(TimersTest, PreciseTimerRegistrationSpeed)
{
    constexpr int timerCount = 10000;
    QVector<QTimer*> timers;
    timers.reserve(timerCount);

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::PreciseTimer);
        t->setInterval(Milliseconds(100));
        t->start();
        timers.push_back(t);
    }

    for (QTimer* t : timers)
        delete t;
}

TEST(TimersTest, CoarseTimerRegistrationSpeed)
{
    constexpr int timerCount = 10000;
    QVector<QTimer*> timers;
    timers.reserve(timerCount);

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::CoarseTimer);
        t->setInterval(Milliseconds(1000000));
        t->start();
        timers.push_back(t);
    }

    for (QTimer* t : timers)
        delete t;
}

TEST(TimersTest, VeryCoarseTimerRegistrationSpeed)
{
    constexpr int timerCount = 10000;
    QVector<QTimer*> timers;
    timers.reserve(timerCount);

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::VeryCoarseTimer);
        t->setInterval(Milliseconds(1000000));
        t->start();
        timers.push_back(t);
    }

    for (QTimer* t : timers)
        delete t;
}

TEST(TimersTest, ManyPreciseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr Milliseconds interval{ 10 };

    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    const TimePoint start_time = Clock::now();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::PreciseTimer);
        t->setInterval(interval);

        QObject::connect(t, &QTimer::timeout, [&]()
        {
            ++triggeredCount;
            if (triggeredCount == timerCount)
                loop.quit();
        });
        t->start();
        timers.append(t);
    }

    loop.exec();
    const qint64 elapsed_ms = msSince(start_time);

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsed_ms, 5000);
}

TEST(TimersTest, ManyCoarseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr Milliseconds interval{ 100 };

    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    const TimePoint start_time = Clock::now();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::CoarseTimer);
        t->setInterval(interval);

        QObject::connect(t, &QTimer::timeout, [&]()
        {
            ++triggeredCount;
            if (triggeredCount == timerCount)
                loop.quit();
        });
        t->start();
        timers.append(t);
    }

    loop.exec();
    const qint64 elapsed_ms = msSince(start_time);

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsed_ms, 5000);
}

TEST(TimersTest, ManyVeryCoarseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr Milliseconds interval{ 100 };

    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    const TimePoint start_time = Clock::now();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::VeryCoarseTimer);
        t->setInterval(interval);

        QObject::connect(t, &QTimer::timeout, [&]()
        {
            ++triggeredCount;
            if (triggeredCount == timerCount)
                loop.quit();
        });
        t->start();
        timers.append(t);
    }

    loop.exec();
    const qint64 elapsed_ms = msSince(start_time);

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsed_ms, 5000);
}

TEST(TimersTest, DISABLED_OnePreciseTimerRepeats)
{
    constexpr Milliseconds requested{ 10 };
    constexpr int repeats = 50;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 3; // +/- 3ms
#else
    constexpr int expectedErrorMs = 10; // +/- 10ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    const TimePoint clock_start = Clock::now();

    QEventLoop loop;
    int count = 0;
    qint64 prev = msSince(clock_start);

    auto* timer = new QTimer();
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(requested);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = msSince(clock_start);
        intervals.append(now - prev); // measure inter-fire interval
        prev = now;

        if (++count >= repeats)
        {
            timer->stop();
            timer->deleteLater();
            loop.quit();
        }
    });

    timer->start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(/* generous */ 60000, &loop, [&]()
    {
        GTEST_LOG_(WARNING) << "Test timeout!";
        loop.quit();
    });

    loop.exec();

    ASSERT_EQ(intervals.size(), repeats);

    const qint64 min = *std::min_element(intervals.begin(), intervals.end());
    const qint64 max = *std::max_element(intervals.begin(), intervals.end());
    const double avg = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();

    GTEST_LOG_(INFO) << "expected: " << requested.count() << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requested.count() + expectedErrorMs);
    EXPECT_GE(min, requested.count() - expectedErrorMs);
}

TEST(TimersTest, DISABLED_OneCoarseTimerRepeats)
{
    constexpr Milliseconds requested{ 100 };
    constexpr int repeats = 50;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 15; // +/- 15ms
#else
    constexpr int expectedErrorMs = 20; // +/- 20ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    const TimePoint clock_start = Clock::now();

    QEventLoop loop;
    int count = 0;
    qint64 prev = msSince(clock_start);

    auto* timer = new QTimer();
    timer->setTimerType(Qt::CoarseTimer);
    timer->setInterval(requested);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = msSince(clock_start);
        intervals.append(now - prev); // measure inter-fire interval
        prev = now;

        if (++count >= repeats)
        {
            timer->stop();
            timer->deleteLater();
            loop.quit();
        }
    });

    timer->start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(/* generous */ 60000, &loop, [&]()
    {
        GTEST_LOG_(WARNING) << "Test timeout!";
        loop.quit();
    });

    loop.exec();

    ASSERT_EQ(intervals.size(), repeats);

    const qint64 min = *std::min_element(intervals.begin(), intervals.end());
    const qint64 max = *std::max_element(intervals.begin(), intervals.end());
    const double avg = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();

    GTEST_LOG_(INFO) << "expected: " << requested.count() << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requested.count() + expectedErrorMs);
    EXPECT_GE(min, requested.count() - expectedErrorMs);
}

TEST(TimersTest, DISABLED_OneVeryCoarseTimerRepeats)
{
    constexpr Milliseconds requested{ 100 };
    constexpr int repeats = 10;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 1000; // +/- 1000ms
#else
    constexpr int expectedErrorMs = 1500; // +/- 1500ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    const TimePoint clock_start = Clock::now();

    QEventLoop loop;
    int count = 0;
    qint64 prev = msSince(clock_start);

    auto* timer = new QTimer();
    timer->setTimerType(Qt::VeryCoarseTimer);
    timer->setInterval(requested);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = msSince(clock_start);
        intervals.append(now - prev); // measure inter-fire interval
        prev = now;

        if (++count >= repeats)
        {
            timer->stop();
            timer->deleteLater();
            loop.quit();
        }
    });

    timer->start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(/* generous */ 60000, &loop, [&]()
    {
        GTEST_LOG_(WARNING) << "Test timeout!";
        loop.quit();
    });

    loop.exec();

    ASSERT_EQ(intervals.size(), repeats);

    // Basic sanity: VeryCoarse should never be "faster" than requested 100ms.
    // (This should always hold; if it doesn't - something's truly off.)
    for (qint64 iv : intervals)
    {
        ASSERT_GE(iv, requested.count());
    }

    const qint64 min = *std::min_element(intervals.begin(), intervals.end());
    const qint64 max = *std::max_element(intervals.begin(), intervals.end());
    const double avg = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();

    GTEST_LOG_(INFO) << "expected: " << requested.count() << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requested.count() + expectedErrorMs);
}

TEST(TimersTest, ManyCoarseAndOneTimerRepeats)
{
    constexpr Milliseconds requested{ 30 };
    constexpr int repeats = 1000;

    QEventLoop loop;
    int count = 0;

    QObject owner;

    for (int i = 0; i < 7000; ++i)
    {
        auto* timer = new QTimer(&owner);
        timer->setTimerType(Qt::CoarseTimer);
        timer->setInterval(Milliseconds(15000));
        timer->setSingleShot(false);
        timer->start();
    }

    auto* timer = new QTimer();
    timer->setTimerType(Qt::CoarseTimer);
    timer->setInterval(requested);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        if (++count >= repeats)
        {
            timer->stop();
            timer->deleteLater();
            loop.quit();
        }
    });

    timer->start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(/* generous */ 60000, &loop, [&]()
    {
        GTEST_LOG_(WARNING) << "Test timeout!";
        loop.quit();
    });

    loop.exec();
}

TEST(TimersTest, ZeroSingleShotTriggering)
{
    constexpr int timerCount = 10000;
    int triggeredCount = 0;

    QEventLoop loop;

    const TimePoint start_time = Clock::now();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer::singleShot(Milliseconds(0), [&]()
        {
            ++triggeredCount;
            if (triggeredCount == timerCount)
                loop.quit();
        });
    }

    loop.exec();
    const qint64 elapsed_ms = msSince(start_time);

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsed_ms, 5000);
}

// A thread stall longer than the timer interval must not cause a burst of catch-up firings:
// missed periods are expected to be coalesced into one, like Qt native dispatchers do.
static void runTimerStallTest(Qt::TimerType type)
{
    constexpr Milliseconds interval{ 50 };
    constexpr int totalTicks = 8;

    QEventLoop loop;
    const TimePoint clock_start = Clock::now();

    QVector<qint64> stamps;
    bool stalled = false;

    QTimer timer;
    timer.setTimerType(type);
    timer.setInterval(interval);

    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        stamps.append(msSince(clock_start));

        // Simulate a stall of several timer periods right after the first tick.
        if (!stalled)
        {
            stalled = true;
            QThread::sleep(interval * 6);
        }

        if (stamps.size() >= totalTicks)
        {
            timer.stop();
            loop.quit();
        }
    });

    timer.start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(Milliseconds(10000), &loop, [&]() { loop.quit(); });
    loop.exec();

    ASSERT_GE(stamps.size(), totalTicks);

    int burstCount = 0;
    for (int i = 1; i < stamps.size(); ++i)
    {
        if (stamps[i] - stamps[i - 1] < interval.count() / 5)
            ++burstCount;
    }

    // One immediate firing right after the stall is fine (that expiration was already due), but
    // the timer must return to its normal cadence instead of replaying every missed period.
    EXPECT_LE(burstCount, 1);
}

TEST(TimersTest, PreciseTimerStallCoalescing)
{
    runTimerStallTest(Qt::PreciseTimer);
}

TEST(TimersTest, CoarseTimerStallCoalescing)
{
    runTimerStallTest(Qt::CoarseTimer);
}

TEST(TimersTest, RemainingTime)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    // Unknown timer id.
    EXPECT_EQ(disp->remainingTime(std::numeric_limits<int>::max()), -1);

    QObject owner;

    // An active timer reports the time left until the first firing.
    const int coarse_id = owner.startTimer(Milliseconds(60000), Qt::CoarseTimer);
    ASSERT_GT(coarse_id, 0);

    const int coarse_remaining = disp->remainingTime(coarse_id);
    EXPECT_GT(coarse_remaining, 0);
    EXPECT_LE(coarse_remaining, 60000);

    // Short precise timers may be backed by a separate native timer mechanism, they must be
    // visible through the same interface.
    const int precise_id = owner.startTimer(Milliseconds(50), Qt::PreciseTimer);
    ASSERT_GT(precise_id, 0);

    const int precise_remaining = disp->remainingTime(precise_id);
    EXPECT_GE(precise_remaining, 0);
    EXPECT_LE(precise_remaining, 50);

    // A timer whose deadline has passed but which has not fired yet (the loop is not pumped)
    // reports zero, not a negative value.
    const int expired_id = owner.startTimer(Milliseconds(10), Qt::CoarseTimer);
    ASSERT_GT(expired_id, 0);

    QThread::msleep(50);
    EXPECT_EQ(disp->remainingTime(expired_id), 0);

    // A killed timer is reported as unknown.
    owner.killTimer(coarse_id);
    EXPECT_EQ(disp->remainingTime(coarse_id), -1);

    owner.killTimer(precise_id);
    owner.killTimer(expired_id);
}

TEST(TimersTest, RegisteredTimersReportEffectiveType)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    QObject owner;
    EXPECT_TRUE(disp->registeredTimers(&owner).isEmpty());

    // Zero-interval and long-interval precise timers are downgraded to coarse by the dispatcher.
    // The effective type must be reported so that a timer moved to another thread is
    // re-registered with the same behavior.
    const int zero_id = owner.startTimer(Milliseconds(0), Qt::PreciseTimer);
    const int long_precise_id = owner.startTimer(Milliseconds(500), Qt::PreciseTimer);
    const int precise_id = owner.startTimer(Milliseconds(50), Qt::PreciseTimer);
    const int coarse_id = owner.startTimer(Milliseconds(200), Qt::CoarseTimer);
    const int very_coarse_id = owner.startTimer(Milliseconds(1500), Qt::VeryCoarseTimer);

    const QList<QAbstractEventDispatcher::TimerInfo> timers = disp->registeredTimers(&owner);
    ASSERT_EQ(timers.size(), 5);

    auto info_for = [&timers](int timer_id) -> const QAbstractEventDispatcher::TimerInfo*
    {
        for (const QAbstractEventDispatcher::TimerInfo& info : timers)
        {
            if (info.timerId == timer_id)
                return &info;
        }
        return nullptr;
    };

    const auto* zero_info = info_for(zero_id);
    ASSERT_NE(zero_info, nullptr);
    EXPECT_EQ(zero_info->timerType, Qt::CoarseTimer);
    EXPECT_EQ(zero_info->interval, 0);

    const auto* long_precise_info = info_for(long_precise_id);
    ASSERT_NE(long_precise_info, nullptr);
    EXPECT_EQ(long_precise_info->timerType, Qt::CoarseTimer);
    EXPECT_EQ(long_precise_info->interval, 500);

    const auto* precise_info = info_for(precise_id);
    ASSERT_NE(precise_info, nullptr);
    EXPECT_EQ(precise_info->timerType, Qt::PreciseTimer);
    EXPECT_EQ(precise_info->interval, 50);

    const auto* coarse_info = info_for(coarse_id);
    ASSERT_NE(coarse_info, nullptr);
    EXPECT_EQ(coarse_info->timerType, Qt::CoarseTimer);
    EXPECT_EQ(coarse_info->interval, 200);

    // Very coarse intervals are rounded up to full seconds by the dispatcher.
    const auto* very_coarse_info = info_for(very_coarse_id);
    ASSERT_NE(very_coarse_info, nullptr);
    EXPECT_EQ(very_coarse_info->timerType, Qt::VeryCoarseTimer);
    EXPECT_GE(very_coarse_info->interval, 1500);
    EXPECT_LE(very_coarse_info->interval, 2000);

    // Timers of another object are not included.
    QObject other;
    EXPECT_TRUE(disp->registeredTimers(&other).isEmpty());

    // unregisterTimers removes everything for the object in one call and reports whether
    // anything was actually removed.
    EXPECT_TRUE(disp->unregisterTimers(&owner));
    EXPECT_TRUE(disp->registeredTimers(&owner).isEmpty());
    EXPECT_FALSE(disp->unregisterTimers(&owner));
}

TEST(TimersTest, HugeIntervalClamped)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    QObject owner;

    // An interval that does not fit into int milliseconds (more than ~24.8 days) must be clamped
    // to INT_MAX in the reporting interfaces instead of overflowing into a negative value.
    const Milliseconds huge_interval(static_cast<qint64>(std::numeric_limits<int>::max()) + 1000000);
    const int timer_id = owner.startTimer(huge_interval, Qt::CoarseTimer);
    ASSERT_GT(timer_id, 0);

    EXPECT_EQ(disp->remainingTime(timer_id), std::numeric_limits<int>::max());

    const QList<QAbstractEventDispatcher::TimerInfo> timers = disp->registeredTimers(&owner);
    ASSERT_EQ(timers.size(), 1);
    EXPECT_EQ(timers[0].timerId, timer_id);
    EXPECT_EQ(timers[0].interval, std::numeric_limits<int>::max());
    EXPECT_EQ(timers[0].timerType, Qt::CoarseTimer);

    owner.killTimer(timer_id);
}

class ED_TimerIdReuseObject : public QObject
{
public:
    static constexpr Milliseconds kInterval{ 100 };

    QEventLoop* loop = nullptr;
    TimePoint start_time;

    int first_id = -1;
    int second_id = -1;
    int first_fires = 0;
    int second_fires = 0;
    bool id_reused = false;
    qint64 second_started_ms = -1;
    qint64 second_first_fire_ms = -1;

protected:
    void timerEvent(QTimerEvent* event) override
    {
        if (event->timerId() == first_id)
        {
            ++first_fires;

            // Kill the fired timer and start a new one with the same id from inside the handler.
            // The dispatcher must treat it as a brand new timer: no rescheduling of the old
            // expiration and no duplicate re-arm on top of the one made at registration. Qt tags
            // reused timer id slots with a serial counter, so the same numeric id comes back only
            // after the serial wraps: cycle kill/start until the fired id is allocated again.
            const int old_id = first_id;
            first_id = -1;
            killTimer(old_id);

            second_started_ms = msSince(start_time);

            int id = startTimer(kInterval, Qt::CoarseTimer);
            for (int i = 0; i < 512 && id != old_id; ++i)
            {
                killTimer(id);
                id = startTimer(kInterval, Qt::CoarseTimer);
            }

            second_id = id;
            id_reused = (second_id == old_id);
        }
        else if (event->timerId() == second_id)
        {
            if (second_fires == 0)
                second_first_fire_ms = msSince(start_time);

            if (++second_fires >= 3)
            {
                killTimer(second_id);
                second_id = -1;
                if (loop)
                    loop->quit();
            }
        }
    }
};

TEST(TimersTest, TimerIdReuseInsideTimerEvent)
{
    QEventLoop loop;

    ED_TimerIdReuseObject obj;
    obj.loop = &loop;
    obj.start_time = Clock::now();

    obj.first_id = obj.startTimer(ED_TimerIdReuseObject::kInterval, Qt::CoarseTimer);
    ASSERT_GT(obj.first_id, 0);

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(Milliseconds(10000), &loop, [&]() { loop.quit(); });
    loop.exec();

    GTEST_LOG_(INFO) << "timer id was " << (obj.id_reused ? "reused" : "not reused");

    EXPECT_EQ(obj.first_fires, 1);
    ASSERT_EQ(obj.second_fires, 3);

    // If the dispatcher confused the new timer with the killed one, the first firing of the new
    // timer is delayed by an extra interval (or lost entirely). The margin is wide enough for a
    // loaded CI, but catches the one-interval shift.
    ASSERT_GE(obj.second_started_ms, 0);
    ASSERT_GE(obj.second_first_fire_ms, 0);
    EXPECT_LT(obj.second_first_fire_ms - obj.second_started_ms,
              ED_TimerIdReuseObject::kInterval.count() * 2 - 10);
}

TEST(SocketTest, EchoClientServer)
{
    constexpr int iterations = 100;
    constexpr quint16 port = 12345;
    const QByteArray testMessage = "Hello, Echo Server!";

    QEventLoop loop;
    int currentIteration = 0;

    std::function<void()> runIteration;

    runIteration = [&]()
    {
        //GTEST_LOG_(INFO) << "Starting iteration " << currentIteration;

        QTcpServer* server = new QTcpServer();
        QTcpSocket* client = new QTcpSocket();
        QByteArray* receiveBuffer = new QByteArray();
        QTimer* watchdog = new QTimer();

        // Watchdog timeout per iteration
        watchdog->setSingleShot(true);
        QObject::connect(watchdog, &QTimer::timeout, [&]()
        {
            GTEST_FAIL() << "Iteration timed out at " << currentIteration;
            loop.quit();
        });
        watchdog->start(Milliseconds(1000)); // 1 second per iteration max

        // Server accepts new connection
        QObject::connect(server, &QTcpServer::newConnection, [=]()
        {
            //GTEST_LOG_(INFO) << "Server: newConnection accepted";
            QTcpSocket* serverSocket = server->nextPendingConnection();

            QObject::connect(serverSocket, &QTcpSocket::readyRead, [=]()
            {
                //GTEST_LOG_(INFO) << "Server: readyRead triggered";
                QByteArray data = serverSocket->readAll();
                //GTEST_LOG_(INFO) << "Server: received " << data.toStdString();
                serverSocket->write(data);
                serverSocket->flush();
            });

            QObject::connect(serverSocket, &QTcpSocket::disconnected, serverSocket, &QObject::deleteLater);

            // Edge case: data arrived before readyRead connected
            if (serverSocket->bytesAvailable() > 0) {
                emit serverSocket->readyRead();
            }
        });

        // Client logic
        QObject::connect(client, &QTcpSocket::connected, [=]()
        {
            QTimer::singleShot(Milliseconds(0), [=]()
            {
                //GTEST_LOG_(INFO) << "Client: sending message";
                client->write(testMessage);
                client->flush();
            });
        });

        QObject::connect(client, &QTcpSocket::readyRead, [=, &loop, &currentIteration, &runIteration]()
        {
            *receiveBuffer += client->readAll();
            //GTEST_LOG_(INFO) << "Client: received chunk, total: " << receiveBuffer->size();

            if (receiveBuffer->size() >= testMessage.size())
            {
                if (*receiveBuffer == testMessage)
                {
                    //GTEST_LOG_(INFO) << "Client: echo successful";

                    watchdog->stop();
                    watchdog->deleteLater();
                    delete receiveBuffer;

                    client->disconnectFromHost();
                    client->deleteLater();
                    server->close();
                    server->deleteLater();

                    ++currentIteration;
                    if (currentIteration >= iterations)
                    {
                        loop.quit();
                    }
                    else
                    {
                        QTimer::singleShot(Milliseconds(0), runIteration);
                    }
                }
                else
                {
                    GTEST_FAIL() << "Incorrect echo: " << receiveBuffer->constData();
                    loop.quit();
                }
            }
        });

        QObject::connect(client, &QTcpSocket::errorOccurred, [=, &loop](QAbstractSocket::SocketError)
        {
            GTEST_FAIL() << "Client error: " << client->errorString().toStdString();
            loop.quit();
        });

        QObject::connect(server, &QTcpServer::acceptError, [=, &loop](QAbstractSocket::SocketError)
        {
            GTEST_FAIL() << "Server error: " << server->errorString().toStdString();
            loop.quit();
        });

        // Start server and connect client
        if (!server->listen(QHostAddress::LocalHost, port))
        {
            GTEST_FAIL() << "Server failed to listen on port " << port;
            loop.quit();
            return;
        }

        client->connectToHost(QHostAddress::LocalHost, port);
    };

    runIteration();
    loop.exec();

    SUCCEED();
}

TEST(SocketTest, ReadNotifierToggleNoDuplicates)
{
    quint16 port_a = 0;
    quint16 port_b = 0;
    NativeSocket sock_a = createBoundUdpSocket(&port_a);
    NativeSocket sock_b = createBoundUdpSocket(&port_b);
    ASSERT_NE(sock_a, kInvalidNativeSocket);
    ASSERT_NE(sock_b, kInvalidNativeSocket);

    // The exception notifier keeps the socket registered in the dispatcher while the read
    // notifier is toggled. Otherwise disabling the read notifier would simply unregister the
    // whole socket, and the toggling would not exercise the pending-wait reuse logic.
    QSocketNotifier exception_notifier(sock_a, QSocketNotifier::Exception);

    int read_count = 0;
    QSocketNotifier read_notifier(sock_a, QSocketNotifier::Read);
    QObject::connect(&read_notifier, &QSocketNotifier::activated, [&]()
    {
        ++read_count;
        drainSocket(sock_a);
    });

    // Let the dispatcher arm the waits before toggling.
    pumpFor(50);

    // Toggle the read notifier while its wait is pending (no data has been sent yet). The
    // dispatcher must reuse the pending wait instead of arming a duplicate one, otherwise each
    // readiness would be delivered multiple times.
    read_notifier.setEnabled(false);
    read_notifier.setEnabled(true);
    read_notifier.setEnabled(false);
    read_notifier.setEnabled(true);

    sendDatagram(sock_b, port_a);
    ASSERT_TRUE(pumpUntil([&]() { return read_count >= 1; }, 5000));

    // Give a possible duplicate delivery time to arrive.
    pumpFor(100);
    EXPECT_EQ(read_count, 1);

    // The wait must have been re-armed after delivery: the next datagram is delivered too.
    sendDatagram(sock_b, port_a);
    ASSERT_TRUE(pumpUntil([&]() { return read_count >= 2; }, 5000));

    pumpFor(100);
    EXPECT_EQ(read_count, 2);

    read_notifier.setEnabled(false);
    exception_notifier.setEnabled(false);
    closeNativeSocket(sock_a);
    closeNativeSocket(sock_b);
}

TEST(SocketTest, WriteNotifierEnabledOnWritableSocket)
{
    quint16 port = 0;
    NativeSocket sock = createBoundUdpSocket(&port);
    ASSERT_NE(sock, kInvalidNativeSocket);

    // Register the socket in the dispatcher without a write notifier and pump the loop. In
    // Windows this makes WSAEnumNetworkEvents consume the initial FD_WRITE edge while there is
    // no write notifier yet - after that no new edge will ever come for this socket unless a
    // send fails with WSAEWOULDBLOCK.
    QSocketNotifier exception_notifier(sock, QSocketNotifier::Exception);
    pumpFor(100);

    int write_count = 0;
    QSocketNotifier write_notifier(sock, QSocketNotifier::Write);
    QObject::connect(&write_notifier, &QSocketNotifier::activated, [&]()
    {
        // Disable on delivery: an enabled write notifier on a writable socket fires on every
        // loop pass, only the fact of delivery matters here.
        ++write_count;
        write_notifier.setEnabled(false);
    });

    // The socket was already writable when the notifier was registered, so no readiness edge
    // will occur. The notification must be delivered anyway.
    ASSERT_TRUE(pumpUntil([&]() { return write_count >= 1; }, 5000));

    pumpFor(100);
    EXPECT_EQ(write_count, 1);

    // Re-enabling on a still writable socket must deliver the notification again.
    write_notifier.setEnabled(true);
    ASSERT_TRUE(pumpUntil([&]() { return write_count >= 2; }, 5000));

    write_notifier.setEnabled(false);
    exception_notifier.setEnabled(false);
    closeNativeSocket(sock);
}

TEST(SocketTest, ReadNotifierEnabledOnReadableSocket)
{
    quint16 port_a = 0;
    quint16 port_b = 0;
    NativeSocket sock_a = createBoundUdpSocket(&port_a);
    NativeSocket sock_b = createBoundUdpSocket(&port_b);
    ASSERT_NE(sock_a, kInvalidNativeSocket);
    ASSERT_NE(sock_b, kInvalidNativeSocket);

    // Register the socket in the dispatcher without a read notifier and deliver a datagram. In
    // Windows this makes WSAEnumNetworkEvents consume the FD_READ event while there is no read
    // notifier yet - no new event will come for this data unless recv is called or more data
    // arrives. The same applies to FD_CLOSE of a TCP socket, which is recorded only once.
    QSocketNotifier exception_notifier(sock_a, QSocketNotifier::Exception);
    pumpFor(100);

    sendDatagram(sock_b, port_a);
    pumpFor(100);

    int read_count = 0;
    QSocketNotifier read_notifier(sock_a, QSocketNotifier::Read);
    QObject::connect(&read_notifier, &QSocketNotifier::activated, [&]()
    {
        ++read_count;
        drainSocket(sock_a);
    });

    // The data was already pending when the notifier was registered, so no readiness event will
    // occur. The notification must be delivered anyway.
    ASSERT_TRUE(pumpUntil([&]() { return read_count >= 1; }, 5000));

    read_notifier.setEnabled(false);
    exception_notifier.setEnabled(false);
    closeNativeSocket(sock_a);
    closeNativeSocket(sock_b);
}
