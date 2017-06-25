//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/synchronization/waitable_event_watcher.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/synchronization/waitable_event_watcher.h"
#include "base/synchronization/waitable_event.h"
#include "base/logging.h"

namespace aspia {

WaitableEventWatcher::WaitableEventWatcher() = default;

WaitableEventWatcher::~WaitableEventWatcher()
{
    StopWatching();
}

bool WaitableEventWatcher::StartWatching(WaitableEvent* event,
                                         EventCallback callback)
{
    callback_ = std::move(callback);
    event_ = event;

    return watcher_.StartWatching(event->Handle(), this);
}

void WaitableEventWatcher::StopWatching()
{
    callback_ = nullptr;
    event_ = nullptr;

    watcher_.StopWatching();
}

void WaitableEventWatcher::OnObjectSignaled(HANDLE object)
{
    WaitableEvent* event = event_;

    event_ = nullptr;
    DCHECK(event);

    callback_();
}

} // namespace aspia
