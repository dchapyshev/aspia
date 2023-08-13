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

#include "base/scoped_task_runner.h"

#include "base/logging.h"

namespace base {

class ScopedTaskRunner::Impl : public base::enable_shared_from_this<Impl>
{
public:
    explicit Impl(std::shared_ptr<TaskRunner> task_runner)
        : task_runner_(std::move(task_runner))
    {
        DCHECK(task_runner_);
    }

    ~Impl()
    {
        dettach();
    }

    void postTask(TaskRunner::Callback callback)
    {
        auto self = shared_from_this();
        task_runner_->postTask([self, callback]()
        {
            if (!self->attached_)
                return;

            callback();
        });
    }

    void postDelayedTask(const std::chrono::milliseconds& timeout, TaskRunner::Callback callback)
    {
        auto self = shared_from_this();
        task_runner_->postDelayedTask([self, callback]()
        {
            if (!self->attached_)
                return;

            callback();
        }, timeout);
    }

    void dettach()
    {
        attached_ = false;
    }

private:
    std::shared_ptr<TaskRunner> task_runner_;
    bool attached_ = true;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
ScopedTaskRunner::ScopedTaskRunner(std::shared_ptr<TaskRunner> task_runner)
    : impl_(base::make_local_shared<Impl>(std::move(task_runner)))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ScopedTaskRunner::~ScopedTaskRunner()
{
    impl_->dettach();
}

//--------------------------------------------------------------------------------------------------
void ScopedTaskRunner::postTask(TaskRunner::Callback callback)
{
    impl_->postTask(std::move(callback));
}

//--------------------------------------------------------------------------------------------------
void ScopedTaskRunner::postDelayedTask(
    const std::chrono::milliseconds& timeout, TaskRunner::Callback callback)
{
    impl_->postDelayedTask(timeout, std::move(callback));
}

} // namespace base
