//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_event.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_EVENT_H
#define _ASPIA_BASE__WAITABLE_EVENT_H

#include "aspia_config.h"
#include "base/macros.h"
#include "base/scoped_handle.h"

namespace aspia {

class WaitableEvent
{
public:
    using NativeHandle = HANDLE;

    WaitableEvent();
    virtual ~WaitableEvent() = default;

    void Notify();
    void Wait(uint32_t timeout_in_ms);
    void Wait();

    NativeHandle native_handle() { return event_; }

private:
    ScopedHandle event_;

    DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_EVENT_H
