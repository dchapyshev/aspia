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

#ifndef BASE_DESKTOP_MAC_FRAME_PROVIDER_H
#define BASE_DESKTOP_MAC_FRAME_PROVIDER_H

#include "base/macros_magic.h"
#include "base/desktop/shared_frame.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"

#include <map>
#include <memory>

#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>

namespace base {

class FrameProvider
{
public:
    explicit FrameProvider(bool allow_iosurface);
    ~FrameProvider();

    // The caller takes ownership of the returned desktop frame. Otherwise returns null if
    // `display_id` is invalid or not ready. Note that this function does not remove the frame from
    // the internal container. Caller has to call the Release function.
    std::unique_ptr<Frame> takeLatestFrameForDisplay(CGDirectDisplayID display_id);

    // OS sends the latest IOSurfaceRef through CGDisplayStreamFrameAvailableHandler callback; we
    // store it here.
    void invalidateIOSurface(CGDirectDisplayID display_id, ScopedCFTypeRef<IOSurfaceRef> io_surface);

    // Expected to be called before stopping the CGDisplayStreamRef streams.
    void release();

private:
    THREAD_CHECKER(thread_checker_);
    const bool allow_iosurface_;

    // Most recent IOSurface that contains a capture of matching display.
    std::map<CGDirectDisplayID, std::unique_ptr<SharedFrame>> io_surfaces_;

    Q_DISABLE_COPY(FrameProvider)
};

} // namespace base

#endif // BASE_DESKTOP_MAC_FRAME_PROVIDER_H
