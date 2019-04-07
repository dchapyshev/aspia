//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/screen_capturer_dfmirage.h"

#include "base/logging.h"
#include "desktop/dfmirage_helper.h"
#include "desktop/desktop_frame_aligned.h"
#include "desktop/win/screen_capture_utils.h"

namespace desktop {

ScreenCapturerDFMirage::ScreenCapturerDFMirage() = default;
ScreenCapturerDFMirage::~ScreenCapturerDFMirage() = default;

bool ScreenCapturerDFMirage::isSupported()
{
    if (!helper_)
    {
        // If DFMirageHelper is not created, then create it.
        helper_ = DFMirageHelper::create(ScreenCaptureUtils::fullScreenRect());
    }

    return helper_ != nullptr;
}

int ScreenCapturerDFMirage::screenCount()
{
    return ScreenCaptureUtils::screenCount();
}

bool ScreenCapturerDFMirage::screenList(ScreenList* screens)
{
    return ScreenCaptureUtils::screenList(screens);
}

bool ScreenCapturerDFMirage::selectScreen(ScreenId screen_id)
{
    if (!ScreenCaptureUtils::isScreenValid(screen_id, &current_device_key_))
        return false;

    reset();

    current_screen_id_ = screen_id;
    return true;
}

const Frame* ScreenCapturerDFMirage::captureFrame()
{
    Rect screen_rect = ScreenCaptureUtils::screenRect(current_screen_id_, current_device_key_);
    if (screen_rect.isEmpty())
    {
        LOG(LS_WARNING) << "Failed to get screen rect";
        return nullptr;
    }

    if (helper_ && helper_->screenRect() != screen_rect)
        reset();

    if (frame_ && frame_->size() != screen_rect.size())
        reset();

    if (!helper_)
    {
        helper_ = DFMirageHelper::create(screen_rect);
        if (!helper_)
        {
            LOG(LS_WARNING) << "Failed to create DFMirage helper";
            return nullptr;
        }
    }

    if (!frame_)
    {
        frame_ = FrameAligned::create(screen_rect.size(), PixelFormat::ARGB(), 32);
        if (!frame_)
        {
            LOG(LS_WARNING) << "Failed to create frame";
            return nullptr;
        }
    }

    DfmChangesBuffer* changes_buffer = helper_->changesBuffer();
    const uint8_t* source_buffer = helper_->screenBuffer();

    Region* region = frame_->updatedRegion();
    region->clear();

    next_update_ = changes_buffer->counter;

    for (int i = last_update_; i != next_update_; i = (i + 1) % kDfmMaxChanges)
    {
        const DfmRect* dfm_rect = &changes_buffer->records[i].rect;

        Rect rect = Rect::makeLTRB(dfm_rect->left, dfm_rect->top, dfm_rect->right, dfm_rect->bottom);

        rect.intersectWith(screen_rect);
        if (!rect.isEmpty())
        {
            region->addRect(rect);

            const size_t source_offset =
                frame_->stride() * rect.y() + frame_->format().bytesPerPixel() * rect.x();

            frame_->copyPixelsFrom(source_buffer + source_offset, frame_->stride(), rect);
        }
    }

    last_update_ = next_update_;

    frame_->setTopLeft(screen_rect.topLeft());
    return frame_.get();
}

void ScreenCapturerDFMirage::reset()
{
    last_update_ = 0;
    next_update_ = 0;

    helper_.reset();
    frame_.reset();
}

} // namespace desktop
