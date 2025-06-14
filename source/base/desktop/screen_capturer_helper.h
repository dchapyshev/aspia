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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_HELPER_H
#define BASE_DESKTOP_SCREEN_CAPTURER_HELPER_H

#include "base/desktop/geometry.h"
#include "base/desktop/region.h"

#include <mutex>

namespace base {

// ScreenCapturerHelper is intended to be used by an implementation of the ScreenCapturer interface.
// It maintains a thread-safe invalid region, and the size of the most recently captured screen, on
// behalf of the ScreenCapturer that owns it.
class ScreenCapturerHelper
{
public:
    ScreenCapturerHelper() = default;
    ~ScreenCapturerHelper() = default;

    // Clear out the invalid region.
    void clearInvalidRegion();

    // Invalidate the specified region.
    void invalidateRegion(const Region& invalid_region);

    // Invalidate the entire screen, of a given size.
    void invalidateScreen(const Size& size);

    // Copies current invalid region to |invalid_region| clears invalid region storage for the next
    // frame.
    void takeInvalidRegion(Region* invalid_region);

    // Access the size of the most recently captured screen.
    const Size& sizeMostRecent() const;
    void setSizeMostRecent(const Size& size);

    // Lossy compression can result in color values leaking between pixels in one block. If part of
    // a block changes, then unchanged parts of that block can be changed in the compressed output.
    // So we need to re-render an entire block whenever part of the block changes.
    //
    // If |log_grid_size| is >= 1, then this function makes takeInvalidRegion() produce an invalid
    // region expanded so that its vertices lie on a grid of size 2 ^ |log_grid_size|. The expanded
    // region is then clipped to the size of the most recently captured screen, as previously set by
    // setSizeMostRecent().
    // If |log_grid_size| is <= 0, then the invalid region is not expanded.
    void setLogGridSize(int log_grid_size);

    // Expands a region so that its vertices all lie on a grid.
    // The grid size must be >= 2, so |log_grid_size| must be >= 1.
    static void expandToGrid(const Region& region, int log_grid_size, Region* result);

private:
    // A region that has been manually invalidated (through InvalidateRegion). These will be
    // returned as dirty_region in the capture data during the next capture.
    Region invalid_region_;

    // A lock protecting |invalid_region_| across threads.
    std::mutex invalid_region_mutex_;

    // The size of the most recently captured screen.
    Size size_most_recent_;

    // The log (base 2) of the size of the grid to which the invalid region is expanded.
    // If the value is <= 0, then the invalid region is not expanded to a grid.
    int log_grid_size_ = 0;

    Q_DISABLE_COPY(ScreenCapturerHelper)
};

} // namespace base

#endif // BASE_DESKTOP_SCREEN_CAPTURER_HELPER_H
