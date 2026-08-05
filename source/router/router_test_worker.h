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

#ifndef ROUTER_ROUTER_TEST_WORKER_H
#define ROUTER_ROUTER_TEST_WORKER_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

#include "base/logging.h"
#include "base/threading/worker.h"
#include "router/database.h"

// A real worker thread of the router: it carries the database connection of its own thread and the
// clock that drives the channels created in it. What is put under test is created, driven and
// destroyed exactly where the router does it - no assumption of the tested code is relaxed to make
// it testable.
class RouterTestWorker final : public Worker
{
public:
    explicit RouterTestWorker(const QString& file_path)
        : Worker(Thread::AsioDispatcher, Seconds(1)),
          file_path_(file_path)
    {
        // Nothing
    }

    ~RouterTestWorker() final = default;

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

    // The connection of the worker thread. Only valid inside invoke().
    Database& database() { return *database_; }

protected:
    // Worker implementation.
    void onStart() final
    {
        database_ = std::make_unique<Database>();
        CHECK(database_->open(file_path_));
    }

    void onStop() final
    {
        database_.reset();
    }

private:
    const QString file_path_;
    std::unique_ptr<Database> database_;

    Q_DISABLE_COPY_MOVE(RouterTestWorker)
};

#endif // ROUTER_ROUTER_TEST_WORKER_H
