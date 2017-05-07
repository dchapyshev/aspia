//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/clipboard_thread.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CLIPBOARD_THREAD_H
#define _ASPIA_HOST__CLIPBOARD_THREAD_H

#include "base/message_loop/message_loop_thread.h"
#include "protocol/clipboard.h"

namespace aspia {

class ClipboardThread : private MessageLoopThread::Delegate
{
public:
    ClipboardThread();
    ~ClipboardThread();

    void Start(Clipboard::ClipboardEventCallback event_callback);
    void InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event);

private:
    // MessageThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    Clipboard::ClipboardEventCallback event_callback_;
    MessageLoopThread ui_thread_;
    Clipboard clipboard_;
    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(ClipboardThread);
};


} // namespace aspia

#endif // _ASPIA_HOST__CLIPBOARD_THREAD_H
