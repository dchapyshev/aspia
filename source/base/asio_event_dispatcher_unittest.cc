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
#include <QElapsedTimer>
#include <QHostInfo>
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

void pumpFor(int duration_ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < duration_ms)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
}

template <typename Predicate>
bool pumpUntil(Predicate condition, int timeout_ms)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_ms)
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
    QElapsedTimer clk;

protected:
    void customEvent(QEvent* ev) override
    {
        if (ev->type() == customType())
        {
            ++received;
            qint64 now = clk.elapsed();
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
    obj.clk.start();

    // Post a bunch of custom events
    constexpr int N = 64;
    for (int i = 0; i < N; ++i)
    {
        QCoreApplication::postEvent(&obj,
            new QEvent(static_cast<QEvent::Type>(ED_TestObject::customType())));
    }

    // Pump the loop until all are processed or timeout
    QElapsedTimer wait; wait.start();
    while (obj.received < N && wait.elapsed() < 2000)
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
    obj.clk.start();

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

    QElapsedTimer wait; wait.start();
    while ((obj.received < N || posted.load() < N) && wait.elapsed() < 4000)
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

// Nested event loops
TEST(DispatcherTests, NestedEventLoops_NoDeadlock)
{
    auto* disp = QAbstractEventDispatcher::instance(QThread::currentThread());
    ASSERT_NE(disp, nullptr);

    bool entered = false;
    bool finished = false;

    QTimer::singleShot(5, [&]()
    {
        entered = true;
        QEventLoop inner;
        QTimer::singleShot(30, &inner, [&](){ inner.quit(); });
        inner.exec();
        finished = true;
    });

    QElapsedTimer wait; wait.start();
    while (!finished && wait.elapsed() < 2000)
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

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::PreciseTimer);
        t->setInterval(100);
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

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::CoarseTimer);
        t->setInterval(1000000);
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

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer *t = new QTimer();
        t->setTimerType(Qt::VeryCoarseTimer);
        t->setInterval(1000000);
        t->start();
        timers.push_back(t);
    }

    for (QTimer* t : timers)
        delete t;
}

TEST(TimersTest, ManyPreciseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr int intervalMs = 10;

    QElapsedTimer elapsed;
    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    elapsed.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::PreciseTimer);
        t->setInterval(intervalMs);

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
    qint64 elapsedMs = elapsed.elapsed();

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsedMs, 5000);
}

TEST(TimersTest, ManyCoarseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr int intervalMs = 100;

    QElapsedTimer elapsed;
    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    elapsed.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::CoarseTimer);
        t->setInterval(intervalMs);

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
    qint64 elapsedMs = elapsed.elapsed();

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsedMs, 5000);
}

TEST(TimersTest, ManyVeryCoarseTimersTriggering)
{
    constexpr int timerCount = 7000;
    constexpr int intervalMs = 100;

    QElapsedTimer elapsed;
    int triggeredCount = 0;

    QEventLoop loop;
    QList<QTimer*> timers;

    elapsed.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer* t = new QTimer();
        t->setSingleShot(true);
        t->setTimerType(Qt::VeryCoarseTimer);
        t->setInterval(intervalMs);

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
    qint64 elapsedMs = elapsed.elapsed();

    for (QTimer* t : timers)
        delete t;

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsedMs, 5000);
}

TEST(TimersTest, DISABLED_OnePreciseTimerRepeats)
{
    constexpr int requestedMs = 10;
    constexpr int repeats = 50;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 3; // +/- 3ms
#else
    constexpr int expectedErrorMs = 10; // +/- 10ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    QElapsedTimer clock;
    clock.start();

    QEventLoop loop;
    int count = 0;
    qint64 prev = clock.elapsed();

    auto* timer = new QTimer();
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(requestedMs);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = clock.elapsed();
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

    GTEST_LOG_(INFO) << "expected: " << requestedMs << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requestedMs + expectedErrorMs);
    EXPECT_GE(min, requestedMs - expectedErrorMs);
}

TEST(TimersTest, DISABLED_OneCoarseTimerRepeats)
{
    constexpr int requestedMs = 100;
    constexpr int repeats = 50;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 15; // +/- 15ms
#else
    constexpr int expectedErrorMs = 20; // +/- 20ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    QElapsedTimer clock;
    clock.start();

    QEventLoop loop;
    int count = 0;
    qint64 prev = clock.elapsed();

    auto* timer = new QTimer();
    timer->setTimerType(Qt::CoarseTimer);
    timer->setInterval(requestedMs);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = clock.elapsed();
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

    GTEST_LOG_(INFO) << "expected: " << requestedMs << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requestedMs + expectedErrorMs);
    EXPECT_GE(min, requestedMs - expectedErrorMs);
}

TEST(TimersTest, DISABLED_OneVeryCoarseTimerRepeats)
{
    constexpr int requestedMs = 100;
    constexpr int repeats = 10;

#if defined(NDEBUG)
    constexpr int expectedErrorMs = 1000; // +/- 1000ms
#else
    constexpr int expectedErrorMs = 1500; // +/- 1500ms
#endif

    QVector<qint64> intervals;
    intervals.reserve(repeats);

    QElapsedTimer clock;
    clock.start();

    QEventLoop loop;
    int count = 0;
    qint64 prev = clock.elapsed();

    auto* timer = new QTimer();
    timer->setTimerType(Qt::VeryCoarseTimer);
    timer->setInterval(requestedMs);
    timer->setSingleShot(false);

    QObject::connect(timer, &QTimer::timeout, [&]()
    {
        const qint64 now = clock.elapsed();
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
        ASSERT_GE(iv, requestedMs);
    }

    const qint64 min = *std::min_element(intervals.begin(), intervals.end());
    const qint64 max = *std::max_element(intervals.begin(), intervals.end());
    const double avg = std::accumulate(intervals.begin(), intervals.end(), 0.0) / intervals.size();

    GTEST_LOG_(INFO) << "expected: " << requestedMs << " ms, min: " << min << " ms, max: " << max
                     << " ms, avg: " << avg << " ms";

    EXPECT_LE(max, requestedMs + expectedErrorMs);
}

TEST(TimersTest, ManyCoarseAndOneTimerRepeats)
{
    constexpr int requestedMs = 30;
    constexpr int repeats = 1000;

    QEventLoop loop;
    int count = 0;

    QObject owner;

    for (int i = 0; i < 7000; ++i)
    {
        auto* timer = new QTimer(&owner);
        timer->setTimerType(Qt::CoarseTimer);
        timer->setInterval(15000);
        timer->setSingleShot(false);
        timer->start();
    }

    auto* timer = new QTimer();
    timer->setTimerType(Qt::CoarseTimer);
    timer->setInterval(requestedMs);
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

    QElapsedTimer elapsed;
    elapsed.start();

    for (int i = 0; i < timerCount; ++i)
    {
        QTimer::singleShot(0, [&]()
        {
            ++triggeredCount;
            if (triggeredCount == timerCount)
                loop.quit();
        });
    }

    loop.exec();
    qint64 elapsedMs = elapsed.elapsed();

    ASSERT_EQ(triggeredCount, timerCount);
    ASSERT_LT(elapsedMs, 5000);
}

// A thread stall longer than the timer interval must not cause a burst of catch-up firings:
// missed periods are expected to be coalesced into one, like Qt native dispatchers do.
static void runTimerStallTest(Qt::TimerType type)
{
    constexpr int intervalMs = 50;
    constexpr int totalTicks = 8;

    QEventLoop loop;
    QElapsedTimer clock;
    clock.start();

    QVector<qint64> stamps;
    bool stalled = false;

    QTimer timer;
    timer.setTimerType(type);
    timer.setInterval(intervalMs);

    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        stamps.append(clock.elapsed());

        // Simulate a stall of several timer periods right after the first tick.
        if (!stalled)
        {
            stalled = true;
            QThread::msleep(intervalMs * 6);
        }

        if (stamps.size() >= totalTicks)
        {
            timer.stop();
            loop.quit();
        }
    });

    timer.start();

    // Failsafe: generous to avoid CI flakiness
    QTimer::singleShot(10000, &loop, [&]() { loop.quit(); });
    loop.exec();

    ASSERT_GE(stamps.size(), totalTicks);

    int burstCount = 0;
    for (int i = 1; i < stamps.size(); ++i)
    {
        if (stamps[i] - stamps[i - 1] < intervalMs / 5)
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
        watchdog->start(1000); // 1 second per iteration max

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
            QTimer::singleShot(0, [=]()
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
                        QTimer::singleShot(0, runIteration);
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
