//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/object_watcher.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/object_watcher.h"

#include "base/logging.h"

namespace aspia {

ObjectWatcher::ObjectWatcher() :
    object_(nullptr),
    wait_object_(nullptr)
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

    if (self)
    {
        self->signal_callback_();
    }
}

bool ObjectWatcher::StartWatching(HANDLE object, const SignalCallback& signal_callback)
{
    if (wait_object_)
    {
        LOG(ERROR) << "Already watching an object";
        return false;
    }

    object_ = object;
    signal_callback_ = signal_callback;

    // Since our job is to just notice when an object is signaled and report the
    // result back to this thread, we can just run on a Windows wait thread.
    DWORD wait_flags = WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE;

    if (!RegisterWaitForSingleObject(&wait_object_,
                                     object_,
                                     DoneWaiting,
                                     this,
                                     INFINITE,
                                     wait_flags))
    {
        LOG(ERROR) << "RegisterWaitForSingleObject() failed: " << GetLastError();
        object_ = nullptr;
        wait_object_ = nullptr;
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

    return true;
}

} // namespace aspia
