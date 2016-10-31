/*
* PROJECT:         Aspia Remote Desktop
* FILE:            capturer_test/screen_updater.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "capturer_test/screen_updater.h"

ScreenUpdater::ScreenUpdater() {}

ScreenUpdater::~ScreenUpdater() {}

void ScreenUpdater::Worker()
{
    scheduler_.reset(new CaptureScheduler());
    capturer_.reset(new CapturerGDI());

    window_.reset(new ScreenWindowWin(nullptr));
    window_->Start();

    DesktopRegion region;
    DesktopSize size;
    PixelFormat format;

    while (!window_->IsEndOfThread())
    {
        scheduler_->BeginCapture();

        const uint8_t *buffer = capturer_->CaptureImage(region, size, format);

        if (!region.is_empty())
        {
            window_->Resize(size, format);

            ScreenWindowWin::LockedMemory memory = window_->GetBuffer();

            for (DesktopRegion::Iterator iter(region); !iter.IsAtEnd(); iter.Advance())
            {
                const DesktopRect &rect = iter.rect();

                int stride = size.width() * format.bytes_per_pixel();
                int offset = stride * rect.top() + rect.left() * format.bytes_per_pixel();

                const uint8_t *src = buffer + offset;
                uint8_t *dst = memory.get() + offset;

                for (int y = 0; y < rect.height(); ++y)
                {
                    memcpy(dst, src, rect.width() * format.bytes_per_pixel());

                    src += stride;
                    dst += stride;
                }
            }

            window_->Invalidate();
        }

        Sleep(scheduler_->NextCaptureDelay());
    }
}

void ScreenUpdater::OnStart() {}

void ScreenUpdater::OnStop() {}
