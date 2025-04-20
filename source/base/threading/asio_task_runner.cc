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

#include "base/threading/asio_task_runner.h"

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

} // namespace

class AsioTaskRunner::Impl final : public QObject
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
AsioTaskRunner::Impl::Impl()
    : current_thread_(QThread::currentThreadId())
{
    DCHECK(QApplication::instance());
}

//--------------------------------------------------------------------------------------------------
AsioTaskRunner::Impl::~Impl() = default;

//--------------------------------------------------------------------------------------------------
bool AsioTaskRunner::Impl::belongsToCurrentThread() const
{
    return QThread::currentThreadId() == current_thread_;
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::Impl::postTask(Callback&& callback, int priority)
{
    QApplication::postEvent(this, new TaskEvent(std::move(callback)), priority);
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::Impl::postDelayedTask(Callback&& callback, const Milliseconds& delay)
{
    QTimer::singleShot(delay.count(), this, [callback]()
    {
        callback();
    });
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::Impl::postQuit()
{
    QTimer::singleShot(0, this, []()
    {
        QThread::currentThread()->quit();
    });
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::Impl::customEvent(QEvent* event)
{
    if (event->type() == TaskEvent::kType)
        reinterpret_cast<TaskEvent*>(event)->callback();
}

//--------------------------------------------------------------------------------------------------
AsioTaskRunner::AsioTaskRunner()
    : impl_(std::make_unique<Impl>())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
AsioTaskRunner::~AsioTaskRunner()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool AsioTaskRunner::belongsToCurrentThread() const
{
    return impl_->belongsToCurrentThread();
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::postTask(Callback callback)
{
    impl_->postTask(std::move(callback), Qt::NormalEventPriority);
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::postDelayedTask(Callback callback, const Milliseconds& delay)
{
    impl_->postDelayedTask(std::move(callback), delay);
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::postNonNestableTask(Callback callback)
{
    impl_->postTask(std::move(callback), Qt::LowEventPriority);
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::postNonNestableDelayedTask(
    Callback /* callback */, const Milliseconds& /* delay */)
{
    NOTIMPLEMENTED();
}

//--------------------------------------------------------------------------------------------------
void AsioTaskRunner::postQuit()
{
    impl_->postQuit();
}

} // namespace base
