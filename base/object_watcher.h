//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/object_watcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__OBJECT_WATCHER_H
#define _ASPIA_BASE__OBJECT_WATCHER_H

#include "aspia_config.h"

#include <functional>

namespace aspia {

class ObjectWatcher
{
public:
    ObjectWatcher();
    ~ObjectWatcher();

    typedef std::function<void()> SignalCallback;

    bool StartWatching(HANDLE object, const SignalCallback& signal_callback);
    bool StopWatching();

private:
    static void NTAPI DoneWaiting(PVOID context, BOOLEAN timed_out);

private:
    SignalCallback signal_callback_;
    HANDLE wait_object_;
    HANDLE object_;
};

} // namespace aspia

#endif // _ASPIA_BASE__OBJECT_WATCHER_H
