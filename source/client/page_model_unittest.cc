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

#include "client/page_model.h"

#include <gtest/gtest.h>

//--------------------------------------------------------------------------------------------------
// An empty list is still one page: the user is always standing on a page, and the window to ask for
// starts at the beginning.
TEST(PageModelTest, EmptyListIsOnePage)
{
    PageModel model;
    model.setPageSize(50);

    EXPECT_EQ(model.pageCount(), 1);
    EXPECT_EQ(model.currentPage(), 0);
    EXPECT_EQ(model.offset(), 0);
}

//--------------------------------------------------------------------------------------------------
TEST(PageModelTest, PagesAreCountedRoundingUp)
{
    PageModel model;
    model.setPageSize(50);

    EXPECT_FALSE(model.setTotalCount(50));
    EXPECT_EQ(model.pageCount(), 1);

    EXPECT_FALSE(model.setTotalCount(51));
    EXPECT_EQ(model.pageCount(), 2);

    EXPECT_FALSE(model.setTotalCount(500));
    EXPECT_EQ(model.pageCount(), 10);
}

//--------------------------------------------------------------------------------------------------
// The window to ask the router for starts where the page starts. A short last page is asked for by
// the full page size, because how short it is is not known before the answer.
TEST(PageModelTest, OffsetFollowsThePage)
{
    PageModel model;
    model.setPageSize(50);
    model.setTotalCount(120);

    EXPECT_EQ(model.offset(), 0);

    model.setCurrentPage(1);
    EXPECT_EQ(model.offset(), 50);

    model.setCurrentPage(2);
    EXPECT_EQ(model.offset(), 100);
    EXPECT_EQ(model.pageSize(), 50);
}

//--------------------------------------------------------------------------------------------------
TEST(PageModelTest, PageOutOfRangeIsClamped)
{
    PageModel model;
    model.setPageSize(50);
    model.setTotalCount(120);

    model.setCurrentPage(100);
    EXPECT_EQ(model.currentPage(), 2);

    model.setCurrentPage(-5);
    EXPECT_EQ(model.currentPage(), 0);
}

//--------------------------------------------------------------------------------------------------
// The last item of the last page was deleted. The page the list was fetched for is gone, so the
// caller is told to ask for the one it landed on instead of showing an empty list.
TEST(PageModelTest, ShrinkingTheListReportsThatThePageMoved)
{
    PageModel model;
    model.setPageSize(50);
    model.setTotalCount(101);
    model.setCurrentPage(2);

    EXPECT_TRUE(model.setTotalCount(100));
    EXPECT_EQ(model.currentPage(), 1);
    EXPECT_EQ(model.offset(), 50);
}

//--------------------------------------------------------------------------------------------------
// A list that lost items but not the page the user is on needs no second request.
TEST(PageModelTest, ShrinkingWithinThePageIsNotAMove)
{
    PageModel model;
    model.setPageSize(50);
    model.setTotalCount(120);
    model.setCurrentPage(2);

    EXPECT_FALSE(model.setTotalCount(105));
    EXPECT_EQ(model.currentPage(), 2);
}

//--------------------------------------------------------------------------------------------------
// Everything is gone, so the only page left is the first one.
TEST(PageModelTest, EmptiedListMovesToTheFirstPage)
{
    PageModel model;
    model.setPageSize(50);
    model.setTotalCount(120);
    model.setCurrentPage(2);

    EXPECT_TRUE(model.setTotalCount(0));
    EXPECT_EQ(model.pageCount(), 1);
    EXPECT_EQ(model.currentPage(), 0);
    EXPECT_EQ(model.offset(), 0);
}

//--------------------------------------------------------------------------------------------------
// A larger page size leaves fewer pages, so the page the user is on can fall off the end of them.
TEST(PageModelTest, PageSizeChangeKeepsThePageInRange)
{
    PageModel model;
    model.setPageSize(25);
    model.setTotalCount(120);
    model.setCurrentPage(4);

    model.setPageSize(100);

    EXPECT_EQ(model.pageCount(), 2);
    EXPECT_EQ(model.currentPage(), 1);
    EXPECT_EQ(model.offset(), 100);
}

//--------------------------------------------------------------------------------------------------
TEST(PageModelTest, ClearStartsOverAndKeepsThePageSize)
{
    PageModel model;
    model.setPageSize(25);
    model.setTotalCount(500);
    model.setCurrentPage(7);

    model.clear();

    EXPECT_EQ(model.totalCount(), 0);
    EXPECT_EQ(model.pageCount(), 1);
    EXPECT_EQ(model.currentPage(), 0);
    EXPECT_EQ(model.pageSize(), 25);
}
