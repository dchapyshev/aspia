//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "desktop_capture/screen_capturer_gdi.h"

#include <QDebug>
#include <dwmapi.h>

#include "desktop_capture/win/screen_capture_utils.h"

namespace aspia {

int ScreenCapturerGDI::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

bool ScreenCapturerGDI::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

bool ScreenCapturerGDI::selectScreen(ScreenId screen_id)
{
    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
        return false;

    // At next screen capture, the resources are recreated.
    desktop_dc_rect_ = DesktopRect();

    current_screen_id_ = screen_id;
    return true;
}

const DesktopFrame* ScreenCapturerGDI::captureFrame()
{
    queue_.moveToNextFrame();

    if (!prepareCaptureResources())
        return nullptr;

    DesktopRect screen_rect = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
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

    current->setTopLeft(screen_rect.leftTop());

    if (!previous || previous->size() != current->size())
    {
        differ_ = std::make_unique<Differ>(screen_rect.size());
        current->updatedRegion()->addRect(DesktopRect::makeSize(screen_rect.size()));
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

    DesktopRect desktop_rect = ScreenCaptureUtils::fullScreenRect();

    // If the display bounds have changed then recreate GDI resources.
    if (desktop_rect != desktop_dc_rect_)
    {
        desktop_dc_.reset();
        memory_dc_.reset();

        desktop_dc_rect_ = DesktopRect();
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
