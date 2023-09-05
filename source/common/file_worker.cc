//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/file_worker.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "common/file_worker_impl.h"
#include "common/file_task.h"

namespace common {

class FileWorker::SharedImpl : public std::enable_shared_from_this<SharedImpl>
{
public:
    explicit SharedImpl(std::shared_ptr<base::TaskRunner> task_runner)
        : task_runner_(std::move(task_runner)),
          impl_(std::make_unique<FileWorkerImpl>())
    {
        DCHECK(task_runner_);
    }

    ~SharedImpl() = default;

    void doTask(std::shared_ptr<FileTask> task)
    {
        auto self = shared_from_this();
        task_runner_->postTask([self, task]()
        {
            std::unique_ptr<proto::FileReply> reply = std::make_unique<proto::FileReply>();
            self->impl_->doRequest(task->request(), reply.get());
            task->setReply(std::move(reply));
        });
    }

    std::shared_ptr<base::TaskRunner> taskRunner() { return task_runner_; }

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<FileWorkerImpl> impl_;

    DISALLOW_COPY_AND_ASSIGN(SharedImpl);
};

//--------------------------------------------------------------------------------------------------
FileWorker::FileWorker(std::shared_ptr<base::TaskRunner> task_runner)
    : impl_(std::make_shared<SharedImpl>(std::move(task_runner)))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
FileWorker::~FileWorker()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileWorker::doTask(std::shared_ptr<FileTask> task)
{
    impl_->doTask(std::move(task));
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::TaskRunner> FileWorker::taskRunner()
{
    return impl_->taskRunner();
}

} // namespace common
