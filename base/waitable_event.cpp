//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_event.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/waitable_event.h"
#include "base/logging.h"

namespace aspia {

WaitableEvent::WaitableEvent() :
    event_(CreateEventW(nullptr, FALSE, FALSE, nullptr))
{
    CHECK(event_) << GetLastError();
}

void WaitableEvent::Notify()
{
    SetEvent(event_);
}

void WaitableEvent::Wait()
{
    Wait(INFINITE);
}

void WaitableEvent::Wait(uint32_t timeout_in_ms)
{
    DWORD ret = WaitForSingleObject(event_, timeout_in_ms);
    DCHECK(ret != WAIT_FAILED) << GetLastError();
}

} // namespace aspia
