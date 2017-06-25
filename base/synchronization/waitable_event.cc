//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/synchronization/waitable_event.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/synchronization/waitable_event.h"
#include "base/logging.h"

namespace aspia {

WaitableEvent::WaitableEvent(ResetPolicy reset_policy, InitialState initial_state) :
    handle_(CreateEventW(nullptr,
                         reset_policy == ResetPolicy::MANUAL,
                         initial_state == InitialState::SIGNALED,
                         nullptr))
{
    // We're probably going to crash anyways if this is ever NULL, so we might as
    // well make our stack reports more informative by crashing here.
    CHECK(handle_.IsValid()) << GetLastSystemErrorString();
}

WaitableEvent::WaitableEvent(ScopedHandle handle)
    : handle_(std::move(handle))
{
    CHECK(handle_.IsValid()) << "Tried to create WaitableEvent from NULL handle";
}

void WaitableEvent::Reset()
{
    ResetEvent(handle_.Get());
}

void WaitableEvent::Signal()
{
    SetEvent(handle_.Get());
}

bool WaitableEvent::IsSignaled()
{
    DWORD result = WaitForSingleObject(handle_.Get(), 0);
    DCHECK(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT)
        << "Unexpected WaitForSingleObject result " << result;
    return result == WAIT_OBJECT_0;
}

void WaitableEvent::Wait()
{
    DWORD result = WaitForSingleObject(handle_, INFINITE);
    // It is most unexpected that this should ever fail.  Help consumers learn
    // about it if it should ever fail.
    DCHECK_EQ(WAIT_OBJECT_0, result) << "WaitForSingleObject failed";
}

bool WaitableEvent::TimedWait(const std::chrono::microseconds& timeout)
{
    DWORD result = WaitForSingleObject(handle_, static_cast<DWORD>(timeout.count()));
    return result != WAIT_TIMEOUT;
}

// static
size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count)
{
    DCHECK(count) << "Cannot wait on no events";

    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    CHECK_LE(count, static_cast<size_t>(MAXIMUM_WAIT_OBJECTS))
        << "Can only wait on " << MAXIMUM_WAIT_OBJECTS << " with WaitMany";

    for (size_t i = 0; i < count; ++i)
        handles[i] = events[i]->Handle();

    // The cast is safe because count is small - see the CHECK above.
    DWORD result =
        WaitForMultipleObjects(static_cast<DWORD>(count),
                               handles,
                               FALSE,      // don't wait for all the objects
                               INFINITE);  // no timeout
    if (result >= WAIT_OBJECT_0 + count)
    {
        LOG(FATAL) << "WaitForMultipleObjects failed";
        return 0;
    }

    return result - WAIT_OBJECT_0;
}

} // namespace aspia
