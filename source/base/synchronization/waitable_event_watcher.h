//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/synchronization/waitable_event_watcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_EVENT_WATCHER_H
#define _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_EVENT_WATCHER_H

#include "base/object_watcher.h"
#include "base/macros.h"

#include <functional>

namespace aspia {

class WaitableEvent;

// This class provides a way to wait on a WaitableEvent asynchronously.
//
// Each instance of this object can be waiting on a single WaitableEvent. When
// the waitable event is signaled, a callback is invoked on the sequence that
// called StartWatching(). This callback can be deleted by deleting the waiter.
//
// Typical usage:
//
//   class MyClass {
//    public:
//     void DoStuffWhenSignaled(WaitableEvent *waitable_event) {
//       watcher_.StartWatching(waitable_event,
//           base::BindOnce(&MyClass::OnWaitableEventSignaled, this);
//     }
//    private:
//     void OnWaitableEventSignaled(WaitableEvent* waitable_event) {
//       // OK, time to do stuff!
//     }
//     base::WaitableEventWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when waitable_event
// becomes signaled. WaitableEventWatcher makes this task easy. When MyClass
// goes out of scope, the watcher_ will be destroyed, and there is no need to
// worry about OnWaitableEventSignaled being called on a deleted MyClass
// pointer.
//
// BEWARE: With automatically reset WaitableEvents, a signal may be lost if it
// occurs just before a WaitableEventWatcher is deleted. There is currently no
// safe way to stop watching an automatic reset WaitableEvent without possibly
// missing a signal.
//
// NOTE: you /are/ allowed to delete the WaitableEvent while still waiting on
// it with a Watcher. It will act as if the event was never signaled.

class WaitableEventWatcher :
    public ObjectWatcher::Delegate
{
public:
    using EventCallback = std::function<void()>;

    WaitableEventWatcher();
    ~WaitableEventWatcher();

    // When |event| is signaled, |callback| is called on the sequence that
    // called StartWatching().
    bool StartWatching(WaitableEvent* event, EventCallback callback);

    // Cancel the current watch. Must be called from the same sequence which
    // started the watch.
    //
    // Does nothing if no event is being watched, nor if the watch has
    // completed.
    // The callback will *not* be called for the current watch after this
    // function returns. Since the callback runs on the same sequence as this
    // function, it cannot be called during this function either.
    void StopWatching();

private:
    // Object watcher implementation.
    void OnObjectSignaled(HANDLE object) override;

    ObjectWatcher watcher_;
    EventCallback callback_;
    WaitableEvent* event_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(WaitableEventWatcher);
};

}  // namespace aspia

#endif // _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_EVENT_WATCHER_H
