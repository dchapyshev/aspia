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

#ifndef RELAY_RELAY_TEST_WORKER_H
#define RELAY_RELAY_TEST_WORKER_H

#include <asio/ip/tcp.hpp>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>

#include "base/logging.h"
#include "base/threading/asio_event_dispatcher.h"
#include "base/threading/worker.h"

// A real worker thread of the relay. Every asio object of a session belongs to the io_context of
// its own thread, so what is put under test is created, driven and destroyed exactly where the
// relay does it.
class RelayTestWorker final : public Worker
{
public:
    RelayTestWorker()
        : Worker(Thread::AsioDispatcher, Seconds(1))
    {
        // Nothing
    }

    ~RelayTestWorker() final = default;

    // Runs |work| in the worker thread and returns once it has finished. The caller is blocked
    // meanwhile, which keeps the test itself sequential.
    void invoke(const std::function<void()>& work)
    {
        std::mutex lock;
        std::condition_variable finished;
        bool done = false;

        post([&]()
        {
            work();

            std::lock_guard guard(lock);
            done = true;
            finished.notify_one();
        });

        std::unique_lock guard(lock);
        finished.wait(guard, [&]() { return done; });
    }

    // A pair of connected sockets on the loopback interface, in place of the two ends a peer
    // connection has in production. Only valid inside invoke().
    static std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket> createSocketPair()
    {
        asio::io_context& io_context = AsioEventDispatcher::ioContext();
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);

        std::error_code error_code;
        asio::ip::tcp::acceptor acceptor(io_context);
        acceptor.open(endpoint.protocol(), error_code);
        CHECK(!error_code);
        acceptor.bind(endpoint, error_code);
        CHECK(!error_code);
        acceptor.listen(1, error_code);
        CHECK(!error_code);

        // The backlog holds the connection, so both blocking calls return at once.
        asio::ip::tcp::socket outer(io_context);
        outer.connect(acceptor.local_endpoint(), error_code);
        CHECK(!error_code);

        asio::ip::tcp::socket inner(io_context);
        acceptor.accept(inner, error_code);
        CHECK(!error_code);

        return { std::move(outer), std::move(inner) };
    }

protected:
    // Worker implementation.
    void onStart() final {}
    void onStop() final {}

private:
    Q_DISABLE_COPY_MOVE(RelayTestWorker)
};

// Counts events raised in the worker thread and lets the test thread wait for them.
class TestLatch
{
public:
    void signal()
    {
        std::lock_guard guard(lock_);
        ++count_;
        condition_.notify_all();
    }

    [[nodiscard]] bool wait(int expected, Seconds timeout)
    {
        std::unique_lock guard(lock_);
        return condition_.wait_for(guard, timeout, [&]() { return count_ >= expected; });
    }

    [[nodiscard]] int count()
    {
        std::lock_guard guard(lock_);
        return count_;
    }

private:
    std::mutex lock_;
    std::condition_variable condition_;
    int count_ = 0;
};

#endif // RELAY_RELAY_TEST_WORKER_H
