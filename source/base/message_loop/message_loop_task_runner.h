//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__MESSAGE_LOOP__MESSAGE_LOOP_TASK_RUNNER_H
#define BASE__MESSAGE_LOOP__MESSAGE_LOOP_TASK_RUNNER_H

#include "base/task_runner.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/pending_task.h"

#include <shared_mutex>

namespace base {

class MessageLoopTaskRunner : public TaskRunner
{
public:
    static std::shared_ptr<TaskRunner> current();

    // TaskRunner implementation.
    bool belongsToCurrentThread() const override;
    void postTask(Callback callback) override;
    void postDelayedTask(Callback callback, Milliseconds delay) override;
    void postNonNestableTask(Callback callback) override;
    void postNonNestableDelayedTask(Callback callback, Milliseconds delay) override;
    void postQuit() override;

private:
    friend class MessageLoop;

    explicit MessageLoopTaskRunner(MessageLoop* loop);

    // Called directly by MessageLoop::~MessageLoop.
    void willDestroyCurrentMessageLoop();

    MessageLoop* loop_;
    mutable std::shared_mutex loop_lock_;
    std::thread::id thread_id_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopTaskRunner);
};

} // namespace base

#endif // BASE__MESSAGE_LOOP__MESSAGE_LOOP_TASK_RUNNER_H
