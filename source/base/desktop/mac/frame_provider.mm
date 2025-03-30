//
// Aspia Project
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

#include "base/desktop/mac/frame_provider.h"

#include "base/desktop/mac/frame_cgimage.h"
#include "base/desktop/mac/frame_iosurface.h"

#include <utility>

namespace base {

//--------------------------------------------------------------------------------------------------
FrameProvider::FrameProvider(bool allow_iosurface)
    : allow_iosurface_(allow_iosurface)
{
    DETACH_FROM_THREAD(thread_checker_);
}

//--------------------------------------------------------------------------------------------------
FrameProvider::~FrameProvider()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    release();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Frame> FrameProvider::takeLatestFrameForDisplay(CGDirectDisplayID display_id)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!allow_iosurface_ || !io_surfaces_[display_id])
    {
        // Regenerate a snapshot. If iosurface is on it will be empty until the
        // stream handler is called.
        return FrameCGImage::createForDisplay(display_id);
    }

    return io_surfaces_[display_id]->share();
}

//--------------------------------------------------------------------------------------------------
void FrameProvider::invalidateIOSurface(CGDirectDisplayID display_id,
                                        ScopedCFTypeRef<IOSurfaceRef> io_surface)
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!allow_iosurface_)
        return;

    std::unique_ptr<FrameIOSurface> desktop_frame_iosurface = FrameIOSurface::wrap(io_surface);

    io_surfaces_[display_id] = desktop_frame_iosurface ?
        SharedFrame::wrap(std::move(desktop_frame_iosurface)) : nullptr;
}

//--------------------------------------------------------------------------------------------------
void FrameProvider::release()
{
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!allow_iosurface_)
        return;

    io_surfaces_.clear();
}

} // namespace base
