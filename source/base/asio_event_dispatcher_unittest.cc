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

#include <QCoreApplication>
#include <QHostInfo>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <gtest/gtest.h>

namespace base {

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

TEST(TimersTest, OnePreciseTimerRepeat100Times)
{
    constexpr int intervalMs = 10;
    constexpr int repeats = 100;

    QVector<qint64> deltas;
    deltas.reserve(repeats);

    QElapsedTimer refClock;
    refClock.start();

    QEventLoop loop;
    int count = 0;

    QTimer* timer = new QTimer();
    timer->setTimerType(Qt::PreciseTimer);
    timer->setInterval(intervalMs);
    timer->setSingleShot(false);

    qint64 expectedTime = refClock.elapsed() + intervalMs;

    QObject::connect(timer, &QTimer::timeout, [=, &count, &deltas, &loop, &expectedTime]() mutable
    {
        qint64 now = refClock.elapsed();
        qint64 delta = now - expectedTime;
        deltas.append(delta);

        ++count;
        expectedTime += intervalMs;

        if (count >= repeats)
        {
            timer->stop();
            timer->deleteLater();
            loop.quit();
        }
    });

    timer->start();

    // Failsafe timeout
    QTimer::singleShot(10000, &loop, [&]()
    {
        GTEST_LOG_(WARNING) << "Test timeout!";
        loop.quit();
    });

    loop.exec();

    ASSERT_EQ(deltas.size(), repeats);

    qint64 min = *std::min_element(deltas.begin(), deltas.end());
    qint64 max = *std::max_element(deltas.begin(), deltas.end());
    double avg = std::accumulate(deltas.begin(), deltas.end(), 0.0) / deltas.size();

    GTEST_LOG_(INFO) << "min: " << min << " ms, max: " << max << " ms, avg: " << avg << " ms";

    ASSERT_LT(std::abs(avg), 5);  // Average drift within ±5 ms
    ASSERT_LT(max, 15);           // No spike above 15 ms
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
    constexpr int iterations = 1000;
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
