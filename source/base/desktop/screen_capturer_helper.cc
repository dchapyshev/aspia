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

#include "base/desktop/screen_capturer_helper.h"

#include <cassert>

namespace base {

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::clearInvalidRegion()
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_.clear();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::invalidateRegion(const Region& invalid_region)
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_.addRegion(invalid_region);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::invalidateScreen(const QSize& size)
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_.addRect(Rect::makeSize(size));
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::takeInvalidRegion(Region* invalid_region)
{
    invalid_region->clear();

    {
        std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
        invalid_region->swap(&invalid_region_);
    }

    if (log_grid_size_ > 0)
    {
        Region expanded_region;
        expandToGrid(*invalid_region, log_grid_size_, &expanded_region);
        expanded_region.swap(invalid_region);

        invalid_region->intersectWith(Rect::makeSize(size_most_recent_));
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::setLogGridSize(int log_grid_size)
{
    log_grid_size_ = log_grid_size;
}

//--------------------------------------------------------------------------------------------------
const QSize& ScreenCapturerHelper::sizeMostRecent() const
{
    return size_most_recent_;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::setSizeMostRecent(const QSize& size)
{
    size_most_recent_ = size;
}

//--------------------------------------------------------------------------------------------------
// Returns the largest multiple of |n| that is <= |x|.
// |n| must be a power of 2. |nMask| is ~(|n| - 1).
static int downToMultiple(int x, int n_mask)
{
    return (x & n_mask);
}

//--------------------------------------------------------------------------------------------------
// Returns the smallest multiple of |n| that is >= |x|.
// |n| must be a power of 2. |nMask| is ~(|n| - 1).
static int upToMultiple(int x, int n, int n_mask)
{
    return ((x + n - 1) & n_mask);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::expandToGrid(const Region& region, int log_grid_size, Region* result)
{
    assert(log_grid_size >= 1);
    int grid_size = 1 << log_grid_size;
    int grid_size_mask = ~(grid_size - 1);

    result->clear();
    for (Region::Iterator it(region); !it.isAtEnd(); it.advance())
    {
        int left = downToMultiple(it.rect().left(), grid_size_mask);
        int right = upToMultiple(it.rect().right(), grid_size, grid_size_mask);
        int top = downToMultiple(it.rect().top(), grid_size_mask);
        int bottom = upToMultiple(it.rect().bottom(), grid_size, grid_size_mask);
        result->addRect(Rect::makeLTRB(left, top, right, bottom));
    }
}

} // namespace base
