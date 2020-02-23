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

#include "qt_base/qt_task_runner.h"

#include "base/logging.h"

#include <QApplication>
#include <QEvent>
#include <QThread>

namespace qt_base {

namespace {

class TaskEvent : public QEvent
{
public:
    static const int kType = QEvent::User + 1;

    explicit TaskEvent(const base::TaskRunner::Callback& callback)
        : QEvent(QEvent::Type(kType)),
          callback(callback)
    {
        // Nothing
    }

    base::TaskRunner::Callback callback;

private:
    DISALLOW_COPY_AND_ASSIGN(TaskEvent);
};

} // namespace

class QtTaskRunner::Impl : public QObject
{
public:
    Impl();
    ~Impl();

    bool belongsToCurrentThread() const;
    void postTask(const Callback& callback, int priority);

protected:
    // QObject implementation.
    void customEvent(QEvent* event) override;

private:
    Qt::HANDLE current_thread_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

QtTaskRunner::Impl::Impl()
    : current_thread_(QThread::currentThreadId())
{
    DCHECK(QApplication::instance());
}

QtTaskRunner::Impl::~Impl() = default;

bool QtTaskRunner::Impl::belongsToCurrentThread() const
{
    return QThread::currentThreadId() == current_thread_;
}

void QtTaskRunner::Impl::postTask(const Callback& callback, int priority)
{
    QApplication::postEvent(this, new TaskEvent(callback), priority);
}

void QtTaskRunner::Impl::customEvent(QEvent* event)
{
    if (event->type() == TaskEvent::kType)
        reinterpret_cast<TaskEvent*>(event)->callback();
}

QtTaskRunner::QtTaskRunner()
    : impl_(std::make_unique<Impl>())
{
    // Nothing
}

QtTaskRunner::~QtTaskRunner() = default;

bool QtTaskRunner::belongsToCurrentThread() const
{
    return impl_->belongsToCurrentThread();
}

void QtTaskRunner::postTask(const Callback& callback)
{
    impl_->postTask(callback, Qt::NormalEventPriority);
}

void QtTaskRunner::postDelayedTask(const Callback& /* callback */, const Milliseconds& /* delay */)
{
    NOTIMPLEMENTED();
}

void QtTaskRunner::postNonNestableTask(const Callback& callback)
{
    impl_->postTask(callback, Qt::LowEventPriority);
}

void QtTaskRunner::postNonNestableDelayedTask(
    const Callback& /* callback */, const Milliseconds& /* delay */)
{
    NOTIMPLEMENTED();
}

void QtTaskRunner::postQuit()
{
    NOTIMPLEMENTED();
}

} // namespace qt_base
