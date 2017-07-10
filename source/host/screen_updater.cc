//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/screen_updater.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/screen_updater.h"

namespace aspia {

static const std::chrono::milliseconds kMinUpdateInterval{ 15 };
static const std::chrono::milliseconds kMaxUpdateInterval{ 100 };

ScreenUpdater::~ScreenUpdater()
{
    Stop();
}

bool ScreenUpdater::StartUpdating(Mode mode,
                                  const std::chrono::milliseconds& interval,
                                  Delegate* delegate)
{
    DCHECK(delegate);

    if (interval < kMinUpdateInterval || interval > kMaxUpdateInterval)
    {
        LOG(ERROR) << "Wrong update interval: " << interval.count();
        return false;
    }

    delegate_ = delegate;
    mode_ = mode;
    interval_ = interval;

    Start();

    return true;
}

void ScreenUpdater::Run()
{
    std::unique_ptr<Capturer> capturer(CapturerGDI::Create());

    if (!capturer)
    {
        delegate_->OnScreenUpdateError();
        return;
    }

    CaptureScheduler scheduler(interval_);

    while (!IsStopping())
    {
        scheduler.BeginCapture();

        const DesktopFrame* screen_frame = capturer->CaptureImage();

        if (!screen_frame)
        {
            delegate_->OnScreenUpdateError();
            break;
        }

        if (!screen_frame->UpdatedRegion().IsEmpty())
        {
            delegate_->OnScreenUpdate(screen_frame);
        }

        if (mode_ == Mode::SCREEN_AND_CURSOR)
        {
            std::unique_ptr<MouseCursor> mouse_cursor(capturer->CaptureCursor());

            if (mouse_cursor)
            {
                delegate_->OnCursorUpdate(std::move(mouse_cursor));
            }
        }

        std::this_thread::sleep_for(scheduler.NextCaptureDelay());
    }
}

} // namespace aspia
