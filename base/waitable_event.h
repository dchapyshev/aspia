//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_event.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_EVENT_H
#define _ASPIA_BASE__WAITABLE_EVENT_H

#include "aspia_config.h"

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "base/scoped_handle.h"

namespace aspia {

class WaitableEvent
{
public:
    WaitableEvent();
    virtual ~WaitableEvent();

    void Notify();
    void WaitForEvent();

private:
    ScopedHandle event_;
    volatile LONG state_;

    DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_EVENT_H
