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

#include "base/desktop/screen_capturer_helper.h"

#include <cassert>

namespace base {

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::clearInvalidRegion()
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_ = QRegion();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::invalidateRegion(const QRegion& invalid_region)
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_ += invalid_region;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::invalidateScreen(const QSize& size)
{
    std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
    invalid_region_ += QRect(QPoint(0, 0), size);
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerHelper::takeInvalidRegion(QRegion* invalid_region)
{
    *invalid_region = QRegion();

    {
        std::scoped_lock scoped_invalid_region_lock(invalid_region_mutex_);
        invalid_region->swap(invalid_region_);
    }

    if (log_grid_size_ > 0)
    {
        QRegion expanded_region;
        expandToGrid(*invalid_region, log_grid_size_, &expanded_region);
        expanded_region.swap(*invalid_region);

        *invalid_region = invalid_region->intersected(QRect(QPoint(0, 0), size_most_recent_));
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
void ScreenCapturerHelper::expandToGrid(const QRegion& region, int log_grid_size, QRegion* result)
{
    assert(log_grid_size >= 1);
    int grid_size = 1 << log_grid_size;
    int grid_size_mask = ~(grid_size - 1);

    *result = QRegion();
    for (const auto& rect : region)
    {
        int left = downToMultiple(rect.left(), grid_size_mask);
        int right = upToMultiple(rect.right(), grid_size, grid_size_mask);
        int top = downToMultiple(rect.top(), grid_size_mask);
        int bottom = upToMultiple(rect.bottom(), grid_size, grid_size_mask);
        *result += QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
    }
}

} // namespace base
