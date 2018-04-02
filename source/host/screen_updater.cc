//
// PROJECT:         Aspia
// FILE:            host/screen_updater.cc
// LICENSE:         GNU Lesser General Public License 2.1
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
    Q_ASSERT(screen_update_callback_ != nullptr);
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
        capturer_ = CapturerGDI::create();
        if (capturer_)
            UpdateScreen();

        return;
    }

    runner_->PostDelayedTask(std::bind(&ScreenUpdater::UpdateScreen, this),
                             scheduler_.nextCaptureDelay(update_interval_));
}

void ScreenUpdater::UpdateScreen()
{
    Q_ASSERT(runner_->BelongsToCurrentThread());

    scheduler_.beginCapture();

    const DesktopFrame* screen_frame = capturer_->captureImage();
    if (screen_frame)
    {
        if (screen_frame->updatedRegion().isEmpty())
            screen_frame = nullptr;

        std::unique_ptr<MouseCursor> mouse_cursor;

        if (mode_ == Mode::SCREEN_WITH_CURSOR)
            mouse_cursor = capturer_->captureCursor();

        if (screen_frame || mouse_cursor)
        {
            screen_update_callback_(screen_frame, std::move(mouse_cursor));
            return;
        }
    }

    runner_->PostDelayedTask(std::bind(&ScreenUpdater::UpdateScreen, this), update_interval_);
}

void ScreenUpdater::OnBeforeThreadRunning()
{
    thread_.SetPriority(Thread::Priority::HIGHEST);

    runner_ = thread_.message_loop_proxy();
    Q_ASSERT(runner_);
}

void ScreenUpdater::OnAfterThreadRunning()
{
    // Nothing
}

} // namespace aspia
