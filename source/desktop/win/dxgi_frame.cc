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

#include "desktop/win/dxgi_frame.h"

#include "base/logging.h"
#include "desktop/desktop_frame_aligned.h"

namespace desktop {

DxgiFrame::DxgiFrame(std::shared_ptr<DxgiDuplicatorController>& controller)
    : context_(controller)
{
    // Nothing
}

DxgiFrame::~DxgiFrame() = default;

bool DxgiFrame::prepare(const Size& size, ScreenCapturer::ScreenId source_id)
{
    if (source_id != source_id_)
    {
        // Once the source has been changed, the entire source should be copied.
        source_id_ = source_id;
        context_.reset();
    }

    if (resolution_tracker_.setResolution(size))
    {
        // Once the output size changed, recreate the SharedFrame.
        frame_.reset();
    }

    if (!frame_)
    {
        std::unique_ptr<Frame> frame = FrameAligned::create(size, PixelFormat::ARGB(), 32);
        if (!frame)
        {
            LOG(LS_WARNING) << "DxgiFrame cannot create a new FrameAligned";
            return false;
        }

        // DirectX capturer won't paint each pixel in the frame due to its one
        // capturer per monitor design. So once the new frame is created, we should
        // clear it to avoid the legacy image to be remained on it. See
        // http://crbug.com/708766.
        DCHECK_EQ(frame->stride(), frame->size().width() * frame->format().bytesPerPixel());
        memset(frame->frameData(), 0, frame->stride() * frame->size().height());

        frame_ = SharedFrame::wrap(std::move(frame));
    }

    return !!frame_;
}

SharedFrame* DxgiFrame::frame() const
{
    DCHECK(frame_);
    return frame_.get();
}

DxgiFrame::Context* DxgiFrame::context()
{
    DCHECK(frame_);
    return &context_;
}

} // namespace desktop
