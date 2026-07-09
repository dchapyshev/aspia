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

#include "base/threading/worker_manager.h"

#include "base/logging.h"

namespace {

WorkerManager* g_instance = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
WorkerManager::WorkerManager()
    : thread_id_(std::this_thread::get_id())
{
    CHECK(!g_instance);
    g_instance = this;
}

//--------------------------------------------------------------------------------------------------
WorkerManager::~WorkerManager()
{
    CHECK(std::this_thread::get_id() == thread_id_);

    LOG(INFO) << "Stopping workers...";

    for (const auto& worker : workers_)
        worker.second->stopSoon();

    {
        std::unique_lock lock(lock_);
        while (running_ != 0)
            condition_.wait(lock);
    }

    workers_.clear();
    g_instance = nullptr;

    LOG(INFO) << "All workers stopped";
}

//--------------------------------------------------------------------------------------------------
// static
WorkerManager& WorkerManager::instance()
{
    CHECK(g_instance);
    return *g_instance;
}

//--------------------------------------------------------------------------------------------------
qint64 WorkerManager::add(std::unique_ptr<Worker> worker)
{
    CHECK(std::this_thread::get_id() == thread_id_);
    CHECK(!started_);
    CHECK(worker);
    CHECK(!worker->parent());

    static qint64 counter = 0;
    ++counter;

    workers_.emplace(counter, std::move(worker));
    return counter;
}

//--------------------------------------------------------------------------------------------------
void WorkerManager::start()
{
    CHECK(std::this_thread::get_id() == thread_id_);
    CHECK(!started_);

    LOG(INFO) << "Starting workers...";

    for (const auto& worker : workers_)
        worker.second->start(this);

    {
        std::unique_lock lock(lock_);
        while (running_ < workers_.size())
            condition_.wait(lock);
    }

    LOG(INFO) << "All workers started";
    started_ = true;
}
