//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/clipboard_thread.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/clipboard_thread.h"
#include "base/logging.h"

namespace aspia {

ClipboardThread::~ClipboardThread()
{
    ui_thread_.Stop();
}

void ClipboardThread::Start(Clipboard::ClipboardEventCallback event_callback)
{
    event_callback_ = std::move(event_callback);

    ui_thread_.Start(MessageLoop::TYPE_UI, this);
}

void ClipboardThread::InjectClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ClipboardThread::InjectClipboardEvent,
                                    this,
                                    clipboard_event));
        return;
    }

    clipboard_.InjectClipboardEvent(clipboard_event);
}

void ClipboardThread::OnBeforeThreadRunning()
{
    if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN))
    {
        LOG(ERROR) << "SetThreadPriority() failed: " << GetLastError();
    }

    runner_ = ui_thread_.message_loop_proxy();
    DCHECK(runner_);

    clipboard_.Start(std::move(event_callback_));
}

void ClipboardThread::OnAfterThreadRunning()
{
    clipboard_.Stop();

    if (!SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END))
    {
        LOG(ERROR) << "SetThreadPriority() failed: " << GetLastError();
    }
}

} // namespace aspia
