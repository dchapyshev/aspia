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

#ifndef BASE_WIN_OBJECT_WATCHER_H
#define BASE_WIN_OBJECT_WATCHER_H

#include "base/macros_magic.h"
#include "base/memory/local_memory.h"

#include <memory>

#include <qt_windows.h>

namespace base {
class TaskRunner;
} // namespace base

namespace base {

// A class that provides a means to asynchronously wait for a Windows object to become signaled.
// It is an abstraction around RegisterWaitForSingleObject that provides a notification callback,
// onObjectSignaled, that runs back on the origin sequence (i.e., the sequence that called
// startWatching).
//
// This class acts like a smart pointer such that when it goes out-of-scope, UnregisterWaitEx is
// automatically called, and any in-flight notification is suppressed.
//
// Typical usage:
//
//   class MyClass : public base::win::ObjectWatcher::Delegate
//   {
//   public:
//       void doStuffWhenSignaled(HANDLE object)
//       {
//           watcher_.StartWatchingOnce(object, this);
//       }
//
//       void onObjectSignaled(HANDLE object) override
//       {
//           // OK, time to do stuff!
//       }
//
//   private:
//       base::win::ObjectWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when object becomes signaled. ObjectWatcher
// makes this task easy. When MyClass goes out of scope, the watcher_ will be destroyed, and there
// is no need to worry about onObjectSignaled being called on a deleted MyClass pointer. Easy!
// If the object is already signaled before being watched, OnObjectSignaled is still called after
// (but not necessarily immediately after) watch is started.
//
class ObjectWatcher
{
public:
    explicit ObjectWatcher(std::shared_ptr<TaskRunner> task_runner);
    ~ObjectWatcher();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called from the sequence that started the watch when a signaled object is detected.
        // To continue watching the object, startWatching must be called again.
        virtual void onObjectSignaled(HANDLE object) = 0;
    };

    // When the object is signaled, the given delegate is notified on the sequence where
    // StartWatchingMultipleTimes is called. The ObjectWatcher is not responsible for deleting the
    // delegate. Returns whether watching was successfully initiated.
    bool startWatchingMultipleTimes(HANDLE object, Delegate* delegate);

    // When the object is signaled, the given delegate is notified on the sequence where
    // startWatchingOnce is called. The ObjectWatcher is not responsible for deleting the delegate.
    // Returns whether watching was successfully initiated.
    bool startWatchingOnce(HANDLE object, Delegate* delegate);

    // Stops watching.  Does nothing if the watch has already completed.  If the watch is still
    // active, then it is canceled, and the associated delegate is not notified.
    // Returns true if the watch was canceled. Otherwise, false is returned.
    bool stopWatching();

    // Returns true if currently watching an object.
    bool isWatching() const;

    // Returns the handle of the object being watched.
    HANDLE watchedObject() const;

private:
    class Impl;
    base::local_shared_ptr<Impl> impl_;

    DISALLOW_COPY_AND_ASSIGN(ObjectWatcher);
};

} // namespace base

#endif // BASE_WIN_OBJECT_WATCHER_H
