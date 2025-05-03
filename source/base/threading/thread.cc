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

#include "base/threading/thread.h"

#include "base/threading/asio_event_dispatcher.h"
#include "base/application.h"
#include "base/task_runner.h"
#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_com_initializer.h"
#endif // defined(Q_OS_WINDOWS)

namespace base {

//--------------------------------------------------------------------------------------------------
Thread::Thread(EventDispatcher dispatcher, Delegate* delegate, QObject* parent)
    : QThread(parent),
      delegate_(delegate)
{
    if (dispatcher == AsioDispatcher)
        setEventDispatcher(new AsioEventDispatcher());
}

//--------------------------------------------------------------------------------------------------
void Thread::stop()
{
    quit();
    wait();
}

//--------------------------------------------------------------------------------------------------
// static
std::shared_ptr<TaskRunner> Thread::currentTaskRunner()
{
    Thread* thread = dynamic_cast<Thread*>(QThread::currentThread());
    if (thread)
        return thread->taskRunner();

    std::shared_ptr<TaskRunner> task_runner = Application::taskRunner();
    CHECK(task_runner);

    return task_runner;
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::TaskRunner> Thread::taskRunner()
{
    return task_runner_;
}

//--------------------------------------------------------------------------------------------------
void Thread::run()
{
    task_runner_ = std::make_shared<TaskRunner>();

#if defined(Q_OS_WINDOWS)
    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.isSucceeded());
#endif // defined(Q_OS_WINDOWS)

    if (delegate_)
        delegate_->onBeforeThreadRunning();

    exec();

    if (delegate_)
        delegate_->onAfterThreadRunning();
}

} // namespace base
