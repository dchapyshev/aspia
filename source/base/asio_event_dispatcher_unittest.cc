//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <gtest/gtest.h>

namespace base {

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

} // namespace base
