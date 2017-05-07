//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/object_watcher.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/object_watcher.h"

#include "base/logging.h"

namespace aspia {

ObjectWatcher::ObjectWatcher() :
    object_(nullptr),
    wait_object_(nullptr),
    delegate_(nullptr)
{
    // Nothing
}

ObjectWatcher::~ObjectWatcher()
{
    StopWatching();
}

// static
void NTAPI ObjectWatcher::DoneWaiting(PVOID context, BOOLEAN timed_out)
{
    ObjectWatcher* self = reinterpret_cast<ObjectWatcher*>(context);

    DCHECK(self);

    if (timed_out)
    {
        self->delegate_->OnObjectTimeout(self->object_);
        return;
    }

    self->delegate_->OnObjectSignaled(self->object_);
}

bool ObjectWatcher::StartWatching(HANDLE object, Delegate* delegate)
{
    return StartWatchingInternal(object, INFINITE, delegate);
}

bool ObjectWatcher::StartTimedWatching(HANDLE object,
                                       const std::chrono::milliseconds& timeout,
                                       Delegate* delegate)
{
    DCHECK(timeout.count() < std::numeric_limits<DWORD>::max());

    return StartWatchingInternal(object, static_cast<DWORD>(timeout.count()), delegate);
}

bool ObjectWatcher::StartWatchingInternal(HANDLE object, DWORD timeout, Delegate* delegate)
{
    if (wait_object_)
    {
        LOG(ERROR) << "Already watching an object";
        return false;
    }

    DCHECK(delegate);

    object_ = object;
    delegate_ = delegate;

    // Since our job is to just notice when an object is signaled and report the
    // result back to this thread, we can just run on a Windows wait thread.
    DWORD wait_flags = WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE;

    if (!RegisterWaitForSingleObject(&wait_object_,
                                     object_,
                                     DoneWaiting,
                                     this,
                                     timeout,
                                     wait_flags))
    {
        LOG(ERROR) << "RegisterWaitForSingleObject() failed: " << GetLastError();
        object_ = nullptr;
        wait_object_ = nullptr;
        delegate_ = nullptr;
        return false;
    }

    return true;
}

bool ObjectWatcher::StopWatching()
{
    if (!wait_object_)
        return false;

    if (!UnregisterWaitEx(wait_object_, INVALID_HANDLE_VALUE))
    {
        LOG(ERROR) << "UnregisterWaitEx() failed: " << GetLastError();
        return false;
    }

    wait_object_ = nullptr;
    object_ = nullptr;
    delegate_ = nullptr;

    return true;
}

bool ObjectWatcher::IsWatching() const
{
    return object_ != nullptr;
}

HANDLE ObjectWatcher::GetWatchedObject() const
{
    return object_;
}

} // namespace aspia
