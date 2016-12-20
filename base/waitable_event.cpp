/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/waitable_event.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "base/waitable_event.h"

#include "base/logging.h"

namespace aspia {

static const LONG kSignaled = 1;
static const LONG kNotSignaled = 0;

WaitableEvent::WaitableEvent() :
    event_(CreateEventW(nullptr, FALSE, FALSE, nullptr)),
    state_(kNotSignaled)
{
    CHECK(event_) << "CreateEventW() failed: " << GetLastError();
}

WaitableEvent::~WaitableEvent()
{
    // Nothing
}

void WaitableEvent::Notify()
{
    if (_InterlockedCompareExchange(&state_, kSignaled, kNotSignaled) == kNotSignaled)
    {
        SetEvent(event_);
    }
}

void WaitableEvent::WaitForEvent()
{
    if (_InterlockedCompareExchange(&state_, kNotSignaled, kSignaled) == kNotSignaled)
    {
        if (WaitForSingleObject(event_, INFINITE) == WAIT_FAILED)
        {
            DLOG(WARNING) << "Unknown waiting error";
        }
    }
}

} // namespace aspia
