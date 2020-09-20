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

#include "base/desktop/win/dxgi_frame.h"

#include "base/logging.h"
#include "base/desktop/frame_aligned.h"
#include "base/desktop/shared_memory_frame.h"

namespace base {

DxgiFrame::DxgiFrame(std::shared_ptr<DxgiDuplicatorController> controller,
                     SharedMemoryFactory* shared_memory_factory)
    : context_(std::move(controller)),
      shared_memory_factory_(shared_memory_factory)
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
        frame_.reset();
    }

    if (!last_frame_size_.has_value() || last_frame_size_ != size)
    {
        // Save the last frame size.
        last_frame_size_.emplace(size);

        // Once the output size changed, recreate the SharedFrame.
        frame_.reset();
    }

    if (!frame_)
    {
        std::unique_ptr<Frame> frame;

        if (shared_memory_factory_)
        {
            frame = SharedMemoryFrame::create(size, shared_memory_factory_);
        }
        else
        {
            frame = FrameAligned::create(size, 32);
        }

        if (!frame)
        {
            LOG(LS_WARNING) << "DxgiFrame cannot create a new DesktopFrame";
            return false;
        }

        const Size& frame_size = frame->size();

        // DirectX capturer won't paint each pixel in the frame due to its one
        // capturer per monitor design. So once the new frame is created, we should
        // clear it to avoid the legacy image to be remained on it. See
        // http://crbug.com/708766.
        DCHECK_EQ(frame->stride(), frame_size.width() * Frame::kBytesPerPixel);
        memset(frame->frameData(), 0, frame->stride() * frame_size.height());

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

} // namespace base
