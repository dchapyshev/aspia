//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

//--------------------------------------------------------------------------------------------------
DxgiFrame::DxgiFrame(std::shared_ptr<DxgiDuplicatorController> controller,
                     SharedMemoryFactory* shared_memory_factory)
    : shared_memory_factory_(shared_memory_factory),
      context_(std::move(controller))
{
    LOG(INFO) << "Ctor" << (shared_memory_factory_ ? "WITH" : "WITHOUT") << "shared memory factory";
}

//--------------------------------------------------------------------------------------------------
DxgiFrame::~DxgiFrame()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool DxgiFrame::prepare(const QSize& size, ScreenCapturer::ScreenId source_id)
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
            frame = SharedMemoryFrame::create(size, PixelFormat::ARGB(), shared_memory_factory_);
            LOG(INFO) << "SharedMemoryFrame created";
        }
        else
        {
            frame = FrameAligned::create(size, PixelFormat::ARGB(), 32);
            LOG(INFO) << "FrameAligned created";
        }

        if (!frame)
        {
            LOG(ERROR) << "DxgiFrame cannot create a new Frame";
            return false;
        }

        frame->setCapturerType(static_cast<quint32>(ScreenCapturer::Type::WIN_DXGI));

        const QSize& frame_size = frame->size();

        // DirectX capturer won't paint each pixel in the frame due to its one
        // capturer per monitor design. So once the new frame is created, we should
        // clear it to avoid the legacy image to be remained on it. See
        // http://crbug.com/708766.
        DCHECK_EQ(frame->stride(), frame_size.width() * frame->format().bytesPerPixel());
        memset(frame->frameData(), 0, static_cast<size_t>(frame->stride() * frame_size.height()));

        frame_.reset(frame.release());
    }

    return !!frame_;
}

//--------------------------------------------------------------------------------------------------
SharedPointer<Frame> DxgiFrame::frame() const
{
    DCHECK(frame_);
    return frame_;
}

//--------------------------------------------------------------------------------------------------
DxgiFrame::Context* DxgiFrame::context()
{
    DCHECK(frame_);
    return &context_;
}

} // namespace base
