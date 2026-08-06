//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/screen_capturer_helper.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

// Extracts the rectangles produced by iterating a region.
std::vector<QRect> rectsOf(const Region& region)
{
    std::vector<QRect> result;
    for (const QRect& rect : region)
        result.push_back(rect);
    return result;
}

//--------------------------------------------------------------------------------------------------
std::vector<QRect> takeRects(ScreenCapturerHelper& helper)
{
    Region region;
    helper.takeInvalidRegion(&region);
    return rectsOf(region);
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, NothingInvalidatedByDefault)
{
    ScreenCapturerHelper helper;

    EXPECT_TRUE(takeRects(helper).empty());
    EXPECT_EQ(helper.sizeMostRecent(), QSize());
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, InvalidRegionsAccumulate)
{
    ScreenCapturerHelper helper;

    helper.invalidateRegion(Region(QRect(0, 0, 10, 10)));
    helper.invalidateRegion(Region(QRect(20, 20, 10, 10)));

    const std::vector<QRect> rects = takeRects(helper);
    ASSERT_EQ(rects.size(), 2U);
    EXPECT_EQ(rects[0], QRect(0, 0, 10, 10));
    EXPECT_EQ(rects[1], QRect(20, 20, 10, 10));
}

//--------------------------------------------------------------------------------------------------
// The region belongs to the frame that took it: the next frame starts from an empty one.
TEST(ScreenCapturerHelperTest, TakeClearsTheRegion)
{
    ScreenCapturerHelper helper;

    helper.invalidateRegion(Region(QRect(0, 0, 10, 10)));
    ASSERT_EQ(takeRects(helper).size(), 1U);

    EXPECT_TRUE(takeRects(helper).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, TakeReplacesTheGivenRegion)
{
    ScreenCapturerHelper helper;
    helper.invalidateRegion(Region(QRect(0, 0, 10, 10)));

    Region region(QRect(100, 100, 10, 10));
    helper.takeInvalidRegion(&region);

    const std::vector<QRect> rects = rectsOf(region);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(0, 0, 10, 10));
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, ClearDropsThePendingRegion)
{
    ScreenCapturerHelper helper;

    helper.invalidateRegion(Region(QRect(0, 0, 10, 10)));
    helper.clearInvalidRegion();

    EXPECT_TRUE(takeRects(helper).empty());
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, InvalidateScreenTakesTheWholeSize)
{
    ScreenCapturerHelper helper;

    helper.invalidateScreen(QSize(100, 50));

    const std::vector<QRect> rects = takeRects(helper);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(0, 0, 100, 50));
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, SizeMostRecentIsStored)
{
    ScreenCapturerHelper helper;

    helper.setSizeMostRecent(QSize(1920, 1080));
    EXPECT_EQ(helper.sizeMostRecent(), QSize(1920, 1080));
}

//--------------------------------------------------------------------------------------------------
// Without a grid the region is passed through untouched - in particular it is not clipped to the
// size of the most recently captured screen.
TEST(ScreenCapturerHelperTest, RegionIsNotExpandedWithoutGrid)
{
    ScreenCapturerHelper helper;
    helper.setSizeMostRecent(QSize(64, 64));

    helper.invalidateRegion(Region(QRect(3, 3, 2, 2)));

    const std::vector<QRect> rects = takeRects(helper);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(3, 3, 2, 2));
}

//--------------------------------------------------------------------------------------------------
// A lossy codec re-renders whole blocks, so the invalid region is expanded to the block grid.
TEST(ScreenCapturerHelperTest, RegionIsExpandedToGrid)
{
    ScreenCapturerHelper helper;
    helper.setSizeMostRecent(QSize(64, 64));
    helper.setLogGridSize(4);

    helper.invalidateRegion(Region(QRect(3, 3, 2, 2)));

    const std::vector<QRect> rects = takeRects(helper);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(0, 0, 16, 16));
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, ExpandedRegionIsClippedToScreen)
{
    ScreenCapturerHelper helper;
    helper.setSizeMostRecent(QSize(20, 20));
    helper.setLogGridSize(4);

    helper.invalidateRegion(Region(QRect(17, 17, 2, 2)));

    const std::vector<QRect> rects = takeRects(helper);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(16, 16, 4, 4));
}

//--------------------------------------------------------------------------------------------------
TEST(ScreenCapturerHelperTest, ExpandToGridKeepsAlignedRects)
{
    Region region(QRect(16, 32, 16, 16));
    Region expanded;

    ScreenCapturerHelper::expandToGrid(region, 4, &expanded);

    const std::vector<QRect> rects = rectsOf(expanded);
    ASSERT_EQ(rects.size(), 1U);
    EXPECT_EQ(rects[0], QRect(16, 32, 16, 16));
}
