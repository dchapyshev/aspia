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

#include "client/search_page_model.h"

#include <gtest/gtest.h>

namespace {

using Slice = SearchPageModel::Slice;

//--------------------------------------------------------------------------------------------------
SearchPageModel modelWith(const QList<qint64>& counts, qint64 page_size)
{
    SearchPageModel model;
    model.setPageSize(page_size);

    for (qint64 count : counts)
        model.addSource(count);

    return model;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Nothing found is still one page: the user is always standing on a page, and asking a source for
// anything would be pointless.
TEST(SearchPageModelTest, EmptyResultIsOnePageWithoutSlices)
{
    SearchPageModel model = modelWith({ 0, 0 }, 50);

    EXPECT_EQ(model.totalCount(), 0);
    EXPECT_EQ(model.pageCount(), 1);
    EXPECT_TRUE(model.currentSlices().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A page that sits inside one source asks that source alone, and the offset is counted from the
// start of the source and not from the start of the whole result.
TEST(SearchPageModelTest, PageInsideOneSourceAsksOnlyThatSource)
{
    SearchPageModel model = modelWith({ 500 }, 50);

    EXPECT_EQ(model.pageCount(), 10);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 0, 50 } }));

    model.setCurrentPage(3);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 150, 50 } }));
}

//--------------------------------------------------------------------------------------------------
// The sources follow one another, so a page can straddle the seam between two of them. Both are
// asked, each for its own part, and the parts come back in source order.
TEST(SearchPageModelTest, PageAcrossASeamAsksBothSources)
{
    SearchPageModel model = modelWith({ 30, 40 }, 50);

    EXPECT_EQ(model.totalCount(), 70);
    EXPECT_EQ(model.pageCount(), 2);

    // First page: all of the first source, then the head of the second.
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 0, 30 }, { 1, 0, 20 } }));

    // Second page: the tail of the second source only.
    model.setCurrentPage(1);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 1, 20, 20 } }));
}

//--------------------------------------------------------------------------------------------------
// A page can span more than two sources when they are small, and sources with no matches drop out
// instead of producing an empty request.
TEST(SearchPageModelTest, PageSpansSeveralSourcesAndSkipsEmptyOnes)
{
    SearchPageModel model = modelWith({ 2, 0, 3, 0, 4 }, 50);

    EXPECT_EQ(model.currentSlices(),
              QList<Slice>({ { 0, 0, 2 }, { 2, 0, 3 }, { 4, 0, 4 } }));
}

//--------------------------------------------------------------------------------------------------
// The last page is a short one, and no source is asked for more than it has.
TEST(SearchPageModelTest, LastPageIsShort)
{
    SearchPageModel model = modelWith({ 55 }, 50);

    EXPECT_EQ(model.pageCount(), 2);

    model.setCurrentPage(1);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 50, 5 } }));
}

//--------------------------------------------------------------------------------------------------
// A page beyond the end is clamped to the last one. Matches disappear between refetches (a host is
// removed, a router goes offline), and the view must not be left blank because of it.
TEST(SearchPageModelTest, PageOutOfRangeIsClamped)
{
    SearchPageModel model = modelWith({ 120 }, 50);

    model.setCurrentPage(100);
    EXPECT_EQ(model.currentPage(), 2);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 100, 20 } }));

    model.setCurrentPage(-5);
    EXPECT_EQ(model.currentPage(), 0);
}

//--------------------------------------------------------------------------------------------------
// A source that failed to answer is simply not registered. The pages of the remaining sources stay
// whole rather than being shifted by a hole of unknown size.
TEST(SearchPageModelTest, MissingSourceLeavesTheOthersWhole)
{
    SearchPageModel model = modelWith({ 30, 40 }, 50);
    model.setCurrentPage(1);

    SearchPageModel without_first = modelWith({ 40 }, 50);

    EXPECT_EQ(without_first.pageCount(), 1);
    EXPECT_EQ(without_first.currentSlices(), QList<Slice>({ { 0, 0, 40 } }));
}

//--------------------------------------------------------------------------------------------------
// A new query starts over: no sources, one page, first page.
TEST(SearchPageModelTest, ClearStartsOver)
{
    SearchPageModel model = modelWith({ 500 }, 50);
    model.setCurrentPage(5);

    model.clear();

    EXPECT_EQ(model.totalCount(), 0);
    EXPECT_EQ(model.pageCount(), 1);
    EXPECT_EQ(model.currentPage(), 0);
    EXPECT_TRUE(model.currentSlices().isEmpty());
}

//--------------------------------------------------------------------------------------------------
// The page size the widget offers is what the windows are cut to.
TEST(SearchPageModelTest, PageSizeDrivesTheWindows)
{
    SearchPageModel model = modelWith({ 25 }, 10);

    EXPECT_EQ(model.pageCount(), 3);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 0, 10 } }));

    model.setCurrentPage(2);
    EXPECT_EQ(model.currentSlices(), QList<Slice>({ { 0, 20, 5 } }));
}
