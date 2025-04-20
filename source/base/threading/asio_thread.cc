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

#include "base/threading/asio_thread.h"

#include "base/threading/asio_event_dispatcher.h"
#include "base/threading/asio_task_runner.h"
#include "base/application.h"
#include "base/logging.h"

#if defined(Q_OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif // defined(Q_OS_WIN)

namespace base {

//--------------------------------------------------------------------------------------------------
AsioThread::AsioThread(EventDispatcher event_dispatcher, Delegate* delegate, QObject* parent)
    : QThread(parent),
      delegate_(delegate)
{
    if (event_dispatcher == EventDispatcher::ASIO)
        setEventDispatcher(new AsioEventDispatcher());
}

//--------------------------------------------------------------------------------------------------
void AsioThread::stop()
{
    quit();
    wait();
}

//--------------------------------------------------------------------------------------------------
// static
std::shared_ptr<TaskRunner> AsioThread::currentTaskRunner()
{
    AsioThread* thread = dynamic_cast<AsioThread*>(QThread::currentThread());
    if (thread)
        return thread->taskRunner();

    std::shared_ptr<TaskRunner> task_runner = Application::taskRunner();
    CHECK(task_runner);

    return task_runner;
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<base::TaskRunner> AsioThread::taskRunner()
{
    return task_runner_;
}

//--------------------------------------------------------------------------------------------------
void AsioThread::run()
{
    task_runner_ = std::make_shared<AsioTaskRunner>();

    win::ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.isSucceeded());

    if (delegate_)
        delegate_->onBeforeThreadRunning();

    exec();

    if (delegate_)
        delegate_->onAfterThreadRunning();
}

} // namespace base
