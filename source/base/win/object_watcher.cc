//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

namespace base::win {

ObjectWatcher::ObjectWatcher() = default;

ObjectWatcher::~ObjectWatcher()
{
    stopWatching();
}

bool ObjectWatcher::startWatchingMultipleTimes(HANDLE object, Delegate* delegate)
{
    return startWatchingInternal(object, delegate, false);
}

bool ObjectWatcher::startWatchingOnce(HANDLE object, Delegate* delegate)
{
    return startWatchingInternal(object, delegate, true);
}

bool ObjectWatcher::stopWatching()
{
    if (!wait_object_)
        return false;

    if (!UnregisterWaitEx(wait_object_, INVALID_HANDLE_VALUE))
    {
        PLOG(LS_ERROR) << "UnregisterWaitEx failed";
        return false;
    }

    reset();
    return true;
}

bool ObjectWatcher::isWatching() const
{
    return object_ != nullptr;
}

HANDLE ObjectWatcher::watchedObject() const
{
    return object_;
}

bool ObjectWatcher::startWatchingInternal(HANDLE object, Delegate* delegate, bool execute_only_once)
{
    DCHECK(delegate);

    if (wait_object_)
    {
        DLOG(LS_ERROR) << "Already watching an object";
        return false;
    }

    object_ = object;
    delegate_ = delegate;

    // Since our job is to just notice when an object is signaled and report the result back to
    // this thread, we can just run on a Windows wait thread.
    DWORD wait_flags = WT_EXECUTEINWAITTHREAD;

    if (execute_only_once)
        wait_flags |= WT_EXECUTEONLYONCE;

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

void ObjectWatcher::reset()
{
    object_ = nullptr;
    wait_object_ = nullptr;
    delegate_ = nullptr;
}

// static
void CALLBACK ObjectWatcher::doneWaiting(PVOID context, BOOLEAN timed_out)
{
    ObjectWatcher* self = reinterpret_cast<ObjectWatcher*>(context);

    DCHECK(self);
    DCHECK(!timed_out);

    self->delegate_->onObjectSignaled(self->object_);
}

} // namespace base::win
