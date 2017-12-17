//
// PROJECT:         Aspia
// FILE:            base/process/process_watcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS__PROCESS_WATCHER_H
#define _ASPIA_BASE__PROCESS__PROCESS_WATCHER_H

#include "base/process/process.h"
#include "base/object_watcher.h"

#include <functional>

namespace aspia {

class ProcessWatcher : private ObjectWatcher::Delegate
{
public:
    ProcessWatcher() = default;
    ~ProcessWatcher();

    using EventCallback = std::function<void()>;

    bool StartWatching(const Process& process, EventCallback callback);
    void StopWatching();

private:
    // ObjectWatcher::Delegate implementation.
    void OnObjectSignaled(HANDLE object) override;

    ObjectWatcher watcher_;
    EventCallback callback_;
    HANDLE process_handle_ = kNullProcessHandle;

    DISALLOW_COPY_AND_ASSIGN(ProcessWatcher);
};

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS__PROCESS_WATCHER_H
