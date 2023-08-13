//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/shared_frame.h"

namespace base {

//--------------------------------------------------------------------------------------------------
SharedFrame::SharedFrame(base::local_shared_ptr<Frame>& frame)
    : Frame(frame->size(), frame->format(), frame->stride(),
            frame->frameData(), frame->sharedMemory()),
      frame_(frame)
{
    copyFrameInfoFrom(*frame_);
}

//--------------------------------------------------------------------------------------------------
SharedFrame::~SharedFrame() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<SharedFrame> SharedFrame::wrap(std::unique_ptr<Frame> desktop_frame)
{
    base::local_shared_ptr<Frame> shared_frame(desktop_frame.release());
    return std::unique_ptr<SharedFrame>(new SharedFrame(shared_frame));
}

//--------------------------------------------------------------------------------------------------
bool SharedFrame::shareFrameWith(const SharedFrame& other) const
{
    return frame_.get() == other.frame_.get();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<SharedFrame> SharedFrame::share()
{
    std::unique_ptr<SharedFrame> result(new SharedFrame(frame_));
    result->copyFrameInfoFrom(*this);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool SharedFrame::isShared()
{
    return frame_.use_count() > 1;
}

//--------------------------------------------------------------------------------------------------
Frame* SharedFrame::underlyingFrame()
{
    return frame_.get();
}

} // namespace base
