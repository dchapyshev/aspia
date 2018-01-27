//
// PROJECT:         Aspia
// FILE:            host/clipboard_thread.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CLIPBOARD_THREAD_H
#define _ASPIA_HOST__CLIPBOARD_THREAD_H

#include "base/threading/thread.h"
#include "protocol/clipboard.h"

namespace aspia {

class ClipboardThread : private Thread::Delegate
{
public:
    ClipboardThread() = default;
    ~ClipboardThread();

    void Start(Clipboard::ClipboardEventCallback event_callback);
    void InjectClipboardEvent(std::shared_ptr<proto::desktop::ClipboardEvent> clipboard_event);

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    Clipboard::ClipboardEventCallback event_callback_;
    Thread ui_thread_;
    Clipboard clipboard_;
    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(ClipboardThread);
};


} // namespace aspia

#endif // _ASPIA_HOST__CLIPBOARD_THREAD_H
