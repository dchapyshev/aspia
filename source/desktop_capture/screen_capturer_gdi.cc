//
// PROJECT:         Aspia
// FILE:            desktop_capture/screen_capturer_gdi.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/screen_capturer_gdi.h"

#include <QDebug>
#include <dwmapi.h>

#include "desktop_capture/win/screen_capture_utils.h"

namespace aspia {

bool ScreenCapturerGDI::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

bool ScreenCapturerGDI::selectScreen(ScreenId screen_id)
{
    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
        return false;

    current_screen_id_ = screen_id;
    return true;
}

const DesktopFrame* ScreenCapturerGDI::captureFrame()
{
    queue_.moveToNextFrame();

    if (!prepareCaptureResources())
        return nullptr;

    QRect screen_rect = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect.isEmpty())
    {
        qWarning("Failed to get screen rect");
        return nullptr;
    }

    if (!queue_.currentFrame() || queue_.currentFrame()->size() != screen_rect.size())
    {
        Q_ASSERT(desktop_dc_);
        Q_ASSERT(memory_dc_);

        std::unique_ptr<DesktopFrame> frame =
            DesktopFrameDIB::create(screen_rect.size(), PixelFormat::ARGB(), memory_dc_);
        if (!frame)
        {
            qWarning("Failed to create frame buffer");
            return nullptr;
        }

        queue_.replaceCurrentFrame(std::move(frame));
    }

    DesktopFrameDIB* current = static_cast<DesktopFrameDIB*>(queue_.currentFrame());
    DesktopFrameDIB* previous = static_cast<DesktopFrameDIB*>(queue_.previousFrame());

    HGDIOBJ old_bitmap = SelectObject(memory_dc_, current->bitmap());
    if (old_bitmap)
    {
        BitBlt(memory_dc_,
               0, 0,
               screen_rect.width(),
               screen_rect.height(),
               *desktop_dc_,
               screen_rect.left(),
               screen_rect.top(),
               CAPTUREBLT | SRCCOPY);

        SelectObject(memory_dc_, old_bitmap);
    }

    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(screen_rect.size());
        *current->updatedRegion() = QRect(QPoint(0, 0), screen_rect.size());
    }
    else
    {
        differ_->calcDirtyRegion(previous->frameData(),
                                 current->frameData(),
                                 current->updatedRegion());
    }

    return current;
}

bool ScreenCapturerGDI::prepareCaptureResources()
{
    // Switch to the desktop receiving user input if different from the
    // current one.
    Desktop input_desktop(Desktop::inputDesktop());

    if (input_desktop.isValid() && !desktop_.isSame(input_desktop))
    {
        // Release GDI resources otherwise SetThreadDesktop will fail.
        desktop_dc_.reset();
        memory_dc_.reset();

        // If SetThreadDesktop() fails, the thread is still assigned a desktop.
        // So we can continue capture screen bits, just from the wrong desktop.
        desktop_.setThreadDesktop(std::move(input_desktop));
    }

    QRect desktop_rect = ScreenCaptureUtils::fullScreenRect();

    // If the display bounds have changed then recreate GDI resources.
    if (desktop_rect != desktop_dc_rect_)
    {
        desktop_dc_.reset();
        memory_dc_.reset();

        desktop_dc_rect_ = QRect();
    }

    if (!desktop_dc_)
    {
        Q_ASSERT(!memory_dc_);

        // Vote to disable Aero composited desktop effects while capturing.
        // Windows will restore Aero automatically if the process exits.
        // This has no effect under Windows 8 or higher. See crbug.com/124018.
        DwmEnableComposition(DWM_EC_DISABLECOMPOSITION);

        // Create GDI device contexts to capture from the desktop into memory.
        desktop_dc_ = std::make_unique<ScopedGetDC>(nullptr);
        memory_dc_.reset(CreateCompatibleDC(*desktop_dc_));
        if (!memory_dc_)
        {
            qWarning("CreateCompatibleDC failed");
            return false;
        }

        desktop_dc_rect_ = desktop_rect;

        // Make sure the frame buffers will be reallocated.
        queue_.reset();
    }

    return true;
}

} // namespace aspia
