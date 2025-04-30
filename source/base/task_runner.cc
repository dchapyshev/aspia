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

#include "base/task_runner.h"

#include "base/logging.h"

#include <QApplication>
#include <QEvent>
#include <QThread>
#include <QTimer>

namespace base {

namespace {

class TaskEvent final : public QEvent
{
public:
    static const int kType = QEvent::User + 1;

    explicit TaskEvent(base::TaskRunner::Callback&& callback)
        : QEvent(QEvent::Type(kType)),
          callback(std::move(callback))
    {
        // Nothing
    }

    base::TaskRunner::Callback callback;

private:
    DISALLOW_COPY_AND_ASSIGN(TaskEvent);
};

class DeleteHelper
{
public:
    DeleteHelper(void(*deleter)(const void*), const void* object)
        : deleter_(deleter),
        object_(object)
    {
        // Nothing
    }

    ~DeleteHelper()
    {
        doDelete();
    }

    void doDelete()
    {
        if (deleter_ && object_)
        {
            deleter_(object_);

            deleter_ = nullptr;
            object_ = nullptr;
        }
    }

private:
    void(*deleter_)(const void*);
    const void* object_;

    DISALLOW_COPY_AND_ASSIGN(DeleteHelper);
};

} // namespace

class TaskRunner::Impl final : public QObject
{
public:
    Impl();
    ~Impl() final;

    bool belongsToCurrentThread() const;
    void postTask(Callback&& callback, int priority);
    void postDelayedTask(Callback&& callback, const Milliseconds& delay);
    void postQuit();

protected:
    // QObject implementation.
    void customEvent(QEvent* event) final;

private:
    Qt::HANDLE current_thread_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
TaskRunner::Impl::Impl()
    : current_thread_(QThread::currentThreadId())
{
    DCHECK(QApplication::instance());
}

//--------------------------------------------------------------------------------------------------
TaskRunner::Impl::~Impl() = default;

//--------------------------------------------------------------------------------------------------
bool TaskRunner::Impl::belongsToCurrentThread() const
{
    return QThread::currentThreadId() == current_thread_;
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::Impl::postTask(Callback&& callback, int priority)
{
    QApplication::postEvent(this, new TaskEvent(std::move(callback)), priority);
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::Impl::postDelayedTask(Callback&& callback, const Milliseconds& delay)
{
    QTimer::singleShot(delay.count(), this, [callback]()
    {
        callback();
    });
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::Impl::postQuit()
{
    QTimer::singleShot(0, this, []()
    {
        QThread::currentThread()->quit();
    });
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::Impl::customEvent(QEvent* event)
{
    if (event->type() == TaskEvent::kType)
        reinterpret_cast<TaskEvent*>(event)->callback();
}

//--------------------------------------------------------------------------------------------------
TaskRunner::TaskRunner()
    : impl_(std::make_unique<Impl>())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
TaskRunner::~TaskRunner()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool TaskRunner::belongsToCurrentThread() const
{
    return impl_->belongsToCurrentThread();
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::postTask(Callback callback)
{
    impl_->postTask(std::move(callback), Qt::NormalEventPriority);
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::postDelayedTask(Callback callback, const Milliseconds& delay)
{
    impl_->postDelayedTask(std::move(callback), delay);
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::postNonNestableTask(Callback callback)
{
    impl_->postTask(std::move(callback), Qt::LowEventPriority);
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::postNonNestableDelayedTask(
    Callback /* callback */, const Milliseconds& /* delay */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::postQuit()
{
    impl_->postQuit();
}

//--------------------------------------------------------------------------------------------------
void TaskRunner::deleteSoonInternal(void(*deleter)(const void*), const void* object)
{
    postNonNestableTask(
        std::bind(&DeleteHelper::doDelete, std::make_shared<DeleteHelper>(deleter, object)));
}

} // namespace base
