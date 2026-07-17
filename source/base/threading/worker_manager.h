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

#ifndef BASE_THREADING_WORKER_MANAGER_H
#define BASE_THREADING_WORKER_MANAGER_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "base/logging.h"
#include "base/threading/worker.h"

class WorkerManager
{
public:
    WorkerManager();
    ~WorkerManager();

    qint64 add(std::unique_ptr<Worker> worker);
    void start();

    template <typename T>
    T* find(qint64 worker_id)
    {
        auto it = workers_.find(worker_id);
        if (it == workers_.end())
            return nullptr;

        return dynamic_cast<T*>(it->second.get());
    }

    // Finds a worker by its type. Safe to call from any worker thread once the manager has started
    // (the worker set does not change afterwards).
    template <typename T>
    T* find()
    {
        for (const auto& worker : workers_)
        {
            if (T* result = dynamic_cast<T*>(worker.second.get()))
                return result;
        }

        return nullptr;
    }

private:
    friend class Worker;

    const std::thread::id thread_id_;

    std::condition_variable condition_;
    std::mutex lock_;
    size_t running_ = 0;
    bool started_ = false;
    qint64 next_worker_id_ = 0;

    std::unordered_map<qint64, std::unique_ptr<Worker>> workers_;

    Q_DISABLE_COPY_MOVE(WorkerManager)
};

// Defined here because it needs the complete WorkerManager type.
template <typename T>
T* Worker::findWorker() const
{
    CHECK(manager_);
    return manager_->find<T>();
}

#endif // BASE_THREADING_WORKER_MANAGER_H
