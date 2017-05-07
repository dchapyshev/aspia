//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_event.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_EVENT_H
#define _ASPIA_BASE__WAITABLE_EVENT_H

#include "base/macros.h"
#include "base/scoped_object.h"

#include <chrono>

namespace aspia {

class WaitableEvent
{
public:
    using NativeHandle = HANDLE;

    // Indicates whether a WaitableEvent should automatically reset the event
    // state after a single waiting thread has been released or remain signaled
    // until Reset() is manually invoked.
    enum class ResetPolicy { Manual, Automatic };

    // Indicates whether a new WaitableEvent should start in a signaled state or
    // not.
    enum class InitialState { Signaled, NotSignaled };

    WaitableEvent(ResetPolicy reset_policy, InitialState initial_state);

    // Create a WaitableEvent from an Event HANDLE which has already been
    // created. This objects takes ownership of the HANDLE and will close it when
    // deleted.
    explicit WaitableEvent(ScopedHandle event_handle);

    virtual ~WaitableEvent() = default;

    // Put the event in the un-signaled state.
    void Reset();

    // Put the event in the signaled state.  Causing any thread blocked on Wait
    // to be woken up.
    void Signal();

    // Returns true if the event is in the signaled state, else false.  If this
    // is not a manual reset event, then this test will cause a reset.
    bool IsSignaled();

    // Wait indefinitely for the event to be signaled. Wait's return "happens
    // after" |Signal| has completed. This means that it's safe for a
    // WaitableEvent to synchronise its own destruction, like this:
    //
    //   WaitableEvent *e = new WaitableEvent;
    //   SendToOtherThread(e);
    //   e->Wait();
    //   delete e;
    void Wait();

    void TimedWait(const std::chrono::microseconds& timeout);

    NativeHandle Handle() { return handle_; }

    // Wait, synchronously, on multiple events.
    //   waitables: an array of WaitableEvent pointers
    //   count: the number of elements in @waitables
    //
    // returns: the index of a WaitableEvent which has been signaled.
    //
    // You MUST NOT delete any of the WaitableEvent objects while this wait is
    // happening, however WaitMany's return "happens after" the |Signal| call
    // that caused it has completed, like |Wait|.
    //
    // If more than one WaitableEvent is signaled to unblock WaitMany, the lowest
    // index among them is returned.
    static size_t WaitMany(WaitableEvent** waitables, size_t count);

private:
    ScopedHandle handle_;

    DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_EVENT_H
