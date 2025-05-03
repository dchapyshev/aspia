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

#include "base/win/object_watcher.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace base {

class ObjectWatcher::Impl : public base::enable_shared_from_this<Impl>
{
public:
    explicit Impl(std::shared_ptr<TaskRunner> task_runner);
    ~Impl();

    bool startWatching(HANDLE object, Delegate* delegate, bool execute_only_once);
    bool stopWatching();
    bool isWatching() const;
    HANDLE watchedObject() const;

private:
    void signal(Delegate* delegate);
    void reset();

    // Called on a background thread when done waiting.
    static void CALLBACK doneWaiting(PVOID context, BOOLEAN timed_out);

    using Callback = std::function<void()>;

    // A callback pre-bound to signal() that is posted to the caller's task runner when the wait
    // completes.
    Callback callback_;

    // The task runner of the sequence on which the watch was started.
    std::shared_ptr<TaskRunner> task_runner_;

    Delegate* delegate_ = nullptr;

    // The wait handle returned by RegisterWaitForSingleObject.
    HANDLE wait_object_ = nullptr;

    // The object being watched.
    HANDLE object_ = nullptr;

    bool run_once_ = true;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
ObjectWatcher::Impl::Impl(std::shared_ptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
    DCHECK(task_runner_->belongsToCurrentThread());
}

//--------------------------------------------------------------------------------------------------
ObjectWatcher::Impl::~Impl()
{
    stopWatching();
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::Impl::startWatching(HANDLE object, Delegate* delegate, bool execute_only_once)
{
    DCHECK(delegate);
    DCHECK(!wait_object_) << "Already watching an object";
    DCHECK(task_runner_->belongsToCurrentThread());

    if (wait_object_)
    {
        LOG(LS_ERROR) << "Already watching an object";
        return false;
    }

    object_ = object;
    delegate_ = delegate;
    run_once_ = execute_only_once;

    // Since our job is to just notice when an object is signaled and report the result back to
    // this thread, we can just run on a Windows wait thread.
    DWORD wait_flags = WT_EXECUTEINWAITTHREAD;
    if (run_once_)
        wait_flags |= WT_EXECUTEONLYONCE;

    callback_ = std::bind(&Impl::signal, shared_from_this(), delegate_);

    if (!RegisterWaitForSingleObject(&wait_object_,
                                     object_,
                                     doneWaiting,
                                     this,
                                     INFINITE,
                                     wait_flags))
    {
        PLOG(LS_ERROR) << "RegisterWaitForSingleObject failed";
        reset();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::Impl::stopWatching()
{
    if (!wait_object_)
        return false;

    DCHECK(task_runner_->belongsToCurrentThread());

    if (!UnregisterWaitEx(wait_object_, INVALID_HANDLE_VALUE))
    {
        PLOG(LS_ERROR) << "UnregisterWaitEx failed";
        return false;
    }

    reset();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::Impl::isWatching() const
{
    return object_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
HANDLE ObjectWatcher::Impl::watchedObject() const
{
    return object_;
}

//--------------------------------------------------------------------------------------------------
void ObjectWatcher::Impl::signal(Delegate* delegate)
{
    if (!isWatching())
        return;

    // Signaling the delegate may result in our destruction or a nested call to startWatching().
    // As a result, we save any state we need and clear previous watcher state before signaling the
    // delegate.
    HANDLE object = object_;

    if (run_once_)
        stopWatching();

    delegate->onObjectSignaled(object);
}

//--------------------------------------------------------------------------------------------------
void ObjectWatcher::Impl::reset()
{
    callback_ = nullptr;
    object_ = nullptr;
    wait_object_ = nullptr;
    delegate_ = nullptr;
    run_once_ = true;
}

//--------------------------------------------------------------------------------------------------
// static
void CALLBACK ObjectWatcher::Impl::doneWaiting(PVOID context, BOOLEAN timed_out)
{
    // The destructor blocks on any callbacks that are in flight, so we know that that is always a
    // pointer to a valid ObjectWater.
    Impl* self = static_cast<Impl*>(context);

    DCHECK(self);
    DCHECK(!timed_out);

    self->task_runner_->postTask(self->callback_);
    if (self->run_once_)
        self->callback_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
ObjectWatcher::ObjectWatcher(std::shared_ptr<TaskRunner> task_runner)
    : impl_(base::make_local_shared<Impl>(std::move(task_runner)))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ObjectWatcher::~ObjectWatcher()
{
    impl_->stopWatching();
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::startWatchingMultipleTimes(HANDLE object, Delegate* delegate)
{
    return impl_->startWatching(object, delegate, false);
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::startWatchingOnce(HANDLE object, Delegate* delegate)
{
    return impl_->startWatching(object, delegate, true);
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::stopWatching()
{
    return impl_->stopWatching();
}

//--------------------------------------------------------------------------------------------------
bool ObjectWatcher::isWatching() const
{
    return impl_->isWatching();
}

//--------------------------------------------------------------------------------------------------
HANDLE ObjectWatcher::watchedObject() const
{
    return impl_->watchedObject();
}

} // namespace base
