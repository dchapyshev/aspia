//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/screen_capturer_mirror.h"

#include "base/logging.h"
#include "desktop/win/dfmirage_helper.h"
#include "desktop/win/mv2_helper.h"
#include "desktop/desktop_frame_aligned.h"
#include "desktop/shared_desktop_frame.h"
#include "desktop/shared_memory_desktop_frame.h"
#include "desktop/win/screen_capture_utils.h"
#include "ipc/shared_memory_factory.h"

namespace desktop {

namespace {

std::unique_ptr<MirrorHelper> createHelper(const Rect& screen_rect)
{
    std::unique_ptr<MirrorHelper> helper = Mv2Helper::create(screen_rect);
    if (!helper)
        helper = DFMirageHelper::create(screen_rect);

    return helper;
}

} // namespace

ScreenCapturerMirror::ScreenCapturerMirror() = default;

ScreenCapturerMirror::~ScreenCapturerMirror() = default;

bool ScreenCapturerMirror::isSupported()
{
    if (!helper_)
        helper_ = createHelper(ScreenCaptureUtils::fullScreenRect());

    return helper_ != nullptr;
}

int ScreenCapturerMirror::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

bool ScreenCapturerMirror::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

bool ScreenCapturerMirror::selectScreen(ScreenId screen_id)
{
    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
        return false;

    reset();

    current_screen_id_ = screen_id;
    return true;
}

const Frame* ScreenCapturerMirror::captureFrame(Error* error)
{
    DCHECK(error);

    *error = prepareCaptureResources();
    if (*error != Error::SUCCEEDED)
        return nullptr;

    Region* updated_region = frame_->updatedRegion();

    // Clearing the region from previous changes.
    updated_region->clear();

    // Update the list of changed areas.
    helper_->addUpdatedRects(updated_region);

    // Exclude a region that should not be captured.
    updated_region->subtract(exclude_region_);

    // Copy the image of the modified areas into the frame.
    helper_->copyRegion(frame_.get(), *updated_region);

    frame_->setTopLeft(helper_->screenRect().topLeft());
    return frame_.get();
}

void ScreenCapturerMirror::reset()
{
    helper_.reset();
    frame_.reset();
}

void ScreenCapturerMirror::updateExcludeRegion()
{
    exclude_region_.clear();

    if (current_screen_id_ != kFullDesktopScreenId)
        return;

    exclude_region_.addRect(desktop_rect_.moved(0, 0));

    ScreenList screen_list;
    if (!ScreenCaptureUtils::screenList(&screen_list))
        return;

    for (const auto& screen : screen_list)
    {
        std::wstring device_key;

        if (ScreenCaptureUtils::isScreenValid(screen.id, &device_key))
        {
            Rect screen_rect = ScreenCaptureUtils::screenRect(screen.id, device_key);

            int x = std::abs(screen_rect.x() - desktop_rect_.x());
            int y = std::abs(screen_rect.y() - desktop_rect_.y());
            int w = screen_rect.width();
            int h = screen_rect.height();

            exclude_region_.subtract(Rect::makeXYWH(x, y, w, h));
        }
    }
}

ScreenCapturerMirror::Error ScreenCapturerMirror::prepareCaptureResources()
{
    Rect desktop_rect = ScreenCaptureUtils::fullScreenRect();
    if (desktop_rect.isEmpty())
    {
        LOG(LS_WARNING) << "Failed to get desktop rect";
        return Error::TEMPORARY;
    }

    if (desktop_rect != desktop_rect_)
    {
        desktop_rect_ = desktop_rect;
        reset();
    }

    Rect screen_rect = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect.isEmpty())
    {
        LOG(LS_WARNING) << "Failed to get screen rect";
        return Error::PERMANENT;
    }

    if (helper_ && helper_->screenRect() != screen_rect)
        reset();

    if (frame_ && frame_->size() != screen_rect.size())
        reset();

    if (!helper_)
    {
        helper_ = createHelper(screen_rect);
        if (!helper_)
        {
            LOG(LS_WARNING) << "Failed to create mirror helper";
            return Error::PERMANENT;
        }

        updateExcludeRegion();
    }

    if (!frame_)
    {
        ipc::SharedMemoryFactory* factory = sharedMemoryFactory();
        if (factory)
        {
            frame_ = SharedMemoryFrame::create(screen_rect.size(), PixelFormat::ARGB(), factory);
        }
        else
        {
            frame_ = FrameAligned::create(screen_rect.size(), PixelFormat::ARGB(), 32);
        }

        if (!frame_)
        {
            LOG(LS_WARNING) << "Failed to create frame";
            return Error::PERMANENT;
        }
    }

    return Error::SUCCEEDED;
}

} // namespace desktop
