//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_updater.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_updater.h"

namespace aspia {

ScreenUpdater::ScreenUpdater(Mode mode,
                             const std::chrono::milliseconds& update_interval,
                             ScreenUpdateCallback screen_update_callback)
    : screen_update_callback_(std::move(screen_update_callback)),
      mode_(mode),
      update_interval_(update_interval)
{
    DCHECK(screen_update_callback_ != nullptr);
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

ScreenUpdater::~ScreenUpdater()
{
    thread_.Stop();
}

void ScreenUpdater::PostUpdateRequest()
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&ScreenUpdater::PostUpdateRequest, this));
        return;
    }

    if (!capturer_)
    {
        capturer_ = CapturerGDI::Create();
        if (capturer_)
            UpdateScreen();

        return;
    }

    std::chrono::milliseconds delay =
        scheduler_.NextCaptureDelay(update_interval_);

    runner_->PostDelayedTask(
        std::bind(&ScreenUpdater::UpdateScreen, this), delay);
}

void ScreenUpdater::UpdateScreen()
{
    DCHECK(runner_->BelongsToCurrentThread());

    scheduler_.BeginCapture();

    const DesktopFrame* screen_frame = capturer_->CaptureImage();
    if (!screen_frame)
        return;

    if (screen_frame->UpdatedRegion().IsEmpty())
        screen_frame = nullptr;

    std::unique_ptr<MouseCursor> mouse_cursor;

    if (mode_ == Mode::SCREEN_AND_CURSOR)
        mouse_cursor = capturer_->CaptureCursor();

    if (screen_frame || mouse_cursor)
    {
        screen_update_callback_(screen_frame, std::move(mouse_cursor));
    }
    else
    {
        runner_->PostDelayedTask(
            std::bind(&ScreenUpdater::UpdateScreen, this), update_interval_);
    }
}

void ScreenUpdater::OnBeforeThreadRunning()
{
    thread_.SetPriority(MessageLoopThread::Priority::HIGHEST);

    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);
}

void ScreenUpdater::OnAfterThreadRunning()
{
    // Nothing
}

} // namespace aspia
