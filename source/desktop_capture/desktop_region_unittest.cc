//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <gtest/gtest.h>

#include <algorithm>

#include "desktop_capture/desktop_region.h"

namespace aspia {

namespace {

int radmonInt(int max)
{
    return (rand() / 256) % max;
}

void compareRegion(const DesktopRegion& region, const DesktopRect rects[], int rects_size)
{
    DesktopRegion::Iterator it(region);
    for (int i = 0; i < rects_size; ++i)
    {
        SCOPED_TRACE(i);
        ASSERT_FALSE(it.isAtEnd());
        EXPECT_TRUE(it.rect().isEqual(rects[i]))
            << it.rect().left() << "-" << it.rect().right() << "."
            << it.rect().top() << "-" << it.rect().bottom() << " "
            << rects[i].left() << "-" << rects[i].right() << "." << rects[i].top()
            << "-" << rects[i].bottom();
        it.advance();
    }
    EXPECT_TRUE(it.isAtEnd());
}

} // namespace

// Verify that regions are empty when created.
TEST(desktop_region_test, empty)
{
    DesktopRegion r;
    compareRegion(r, nullptr, 0);
}

// Verify that empty rectangles are ignored.
TEST(desktop_region_test, add_empty)
{
    DesktopRegion r;
    DesktopRect rect = DesktopRect::makeXYWH(1, 2, 0, 0);
    r.addRect(rect);
    compareRegion(r, nullptr, 0);
}

// Verify that regions with a single rectangles are handled properly.
TEST(desktop_region_test, single_rect)
{
    DesktopRegion r;
    DesktopRect rect = DesktopRect::makeXYWH(1, 2, 3, 4);
    r.addRect(rect);
    compareRegion(r, &rect, 1);
}

// Verify that non-overlapping rectangles are not merged.
TEST(desktop_region_test, non_overlapping_rects)
{
    struct Case
    {
        int count;
        DesktopRect rects[4];
    } cases[] =
    {
        {1, {DesktopRect::makeXYWH(10, 10, 10, 10)}},
        {2,
         {DesktopRect::makeXYWH(10, 10, 10, 10),
          DesktopRect::makeXYWH(30, 10, 10, 15)}},
        {2,
         {DesktopRect::makeXYWH(10, 10, 10, 10),
          DesktopRect::makeXYWH(10, 30, 10, 5)}},
        {3,
         {DesktopRect::makeXYWH(10, 10, 10, 9),
          DesktopRect::makeXYWH(30, 10, 15, 10),
          DesktopRect::makeXYWH(10, 30, 8, 10)}},
        {4,
         {DesktopRect::makeXYWH(0, 0, 30, 10),
          DesktopRect::makeXYWH(40, 0, 10, 30),
          DesktopRect::makeXYWH(0, 20, 10, 30),
          DesktopRect::makeXYWH(20, 40, 30, 10)}},
        {4,
         {DesktopRect::makeXYWH(0, 0, 10, 100),
          DesktopRect::makeXYWH(20, 10, 30, 10),
          DesktopRect::makeXYWH(20, 30, 30, 10),
          DesktopRect::makeXYWH(20, 50, 30, 10)}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r;

        for (int j = 0; j < cases[i].count; ++j)
        {
            r.addRect(cases[i].rects[j]);
        }
        compareRegion(r, cases[i].rects, cases[i].count);

        SCOPED_TRACE("Reverse");

        // Try inserting rects in reverse order.
        r.clear();
        for (int j = cases[i].count - 1; j >= 0; --j)
        {
            r.addRect(cases[i].rects[j]);
        }
        compareRegion(r, cases[i].rects, cases[i].count);
    }
}

TEST(desktop_region_test, two_rects)
{
    struct Case
    {
        DesktopRect input_rect1;
        DesktopRect input_rect2;
        int expected_count;
        DesktopRect expected_rects[3];
    } cases[] = {
        // Touching rectangles that merge into one.
        {DesktopRect::makeLTRB(100, 100, 200, 200),
         DesktopRect::makeLTRB(0, 100, 100, 200),
         1,
         {DesktopRect::makeLTRB(0, 100, 200, 200)}},

        {DesktopRect::makeLTRB(100, 100, 200, 200),
         DesktopRect::makeLTRB(100, 0, 200, 100),
         1,
         {DesktopRect::makeLTRB(100, 0, 200, 200)}},

         // Rectangles touching on the vertical edge.
         {DesktopRect::makeLTRB(100, 100, 200, 200),
          DesktopRect::makeLTRB(0, 150, 100, 250),
          3,
          {DesktopRect::makeLTRB(100, 100, 200, 150),
           DesktopRect::makeLTRB(0, 150, 200, 200),
           DesktopRect::makeLTRB(0, 200, 100, 250)}},
         {DesktopRect::makeLTRB(100, 100, 200, 200),
          DesktopRect::makeLTRB(0, 50, 100, 150),
          3,
          {DesktopRect::makeLTRB(0, 50, 100, 100),
           DesktopRect::makeLTRB(0, 100, 200, 150),
           DesktopRect::makeLTRB(100, 150, 200, 200)}},
         {DesktopRect::makeLTRB(100, 100, 200, 200),
          DesktopRect::makeLTRB(0, 120, 100, 180),
          3,
          {DesktopRect::makeLTRB(100, 100, 200, 120),
           DesktopRect::makeLTRB(0, 120, 200, 180),
           DesktopRect::makeLTRB(100, 180, 200, 200)}},

           // Rectangles touching on the horizontal edge.
           {DesktopRect::makeLTRB(100, 100, 200, 200),
            DesktopRect::makeLTRB(150, 0, 250, 100),
            2,
            {DesktopRect::makeLTRB(150, 0, 250, 100),
             DesktopRect::makeLTRB(100, 100, 200, 200)}},
           {DesktopRect::makeLTRB(100, 100, 200, 200),
            DesktopRect::makeLTRB(50, 0, 150, 100),
            2,
            {DesktopRect::makeLTRB(50, 0, 150, 100),
             DesktopRect::makeLTRB(100, 100, 200, 200)}},
           {DesktopRect::makeLTRB(100, 100, 200, 200),
            DesktopRect::makeLTRB(120, 0, 180, 100),
            2,
            {DesktopRect::makeLTRB(120, 0, 180, 100),
             DesktopRect::makeLTRB(100, 100, 200, 200)}},

             // Overlapping rectangles.
             {DesktopRect::makeLTRB(100, 100, 200, 200),
              DesktopRect::makeLTRB(50, 50, 150, 150),
              3,
              {DesktopRect::makeLTRB(50, 50, 150, 100),
               DesktopRect::makeLTRB(50, 100, 200, 150),
               DesktopRect::makeLTRB(100, 150, 200, 200)}},
             {DesktopRect::makeLTRB(100, 100, 200, 200),
              DesktopRect::makeLTRB(150, 50, 250, 150),
              3,
              {DesktopRect::makeLTRB(150, 50, 250, 100),
               DesktopRect::makeLTRB(100, 100, 250, 150),
               DesktopRect::makeLTRB(100, 150, 200, 200)}},
             {DesktopRect::makeLTRB(100, 100, 200, 200),
              DesktopRect::makeLTRB(0, 120, 150, 180),
              3,
              {DesktopRect::makeLTRB(100, 100, 200, 120),
               DesktopRect::makeLTRB(0, 120, 200, 180),
               DesktopRect::makeLTRB(100, 180, 200, 200)}},
             {DesktopRect::makeLTRB(100, 100, 200, 200),
              DesktopRect::makeLTRB(120, 0, 180, 150),
              2,
              {DesktopRect::makeLTRB(120, 0, 180, 100),
               DesktopRect::makeLTRB(100, 100, 200, 200)}},
             {DesktopRect::makeLTRB(100, 0, 200, 300),
              DesktopRect::makeLTRB(0, 100, 300, 200),
              3,
              {DesktopRect::makeLTRB(100, 0, 200, 100),
               DesktopRect::makeLTRB(0, 100, 300, 200),
               DesktopRect::makeLTRB(100, 200, 200, 300)}},

               // One rectangle enclosing another.
               {DesktopRect::makeLTRB(100, 100, 200, 200),
                DesktopRect::makeLTRB(150, 150, 180, 180),
                1,
                {DesktopRect::makeLTRB(100, 100, 200, 200)}},
               {DesktopRect::makeLTRB(100, 100, 200, 200),
                DesktopRect::makeLTRB(100, 100, 180, 180),
                1,
                {DesktopRect::makeLTRB(100, 100, 200, 200)}},
               {DesktopRect::makeLTRB(100, 100, 200, 200),
                DesktopRect::makeLTRB(150, 150, 200, 200),
                1,
                {DesktopRect::makeLTRB(100, 100, 200, 200)}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r;

        r.addRect(cases[i].input_rect1);
        r.addRect(cases[i].input_rect2);
        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);

        SCOPED_TRACE("Reverse");

        // Run the same test with rectangles inserted in reverse order.
        r.clear();
        r.addRect(cases[i].input_rect2);
        r.addRect(cases[i].input_rect1);
        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);
    }
}

// Verify that DesktopRegion::AddRectToRow() works correctly by creating a row
// of not overlapping rectangles and insert an overlapping rectangle into the
// row at different positions. Result is verified by building a map of the
// region in an array and comparing it with the expected values.
TEST(desktop_region_test, same_row)
{
    const int kMapWidth = 50;
    const int kLastRectSizes[] = { 3, 27 };

    DesktopRegion base_region;
    bool base_map[kMapWidth] = { false };

    base_region.addRect(DesktopRect::makeXYWH(5, 0, 5, 1));
    std::fill_n(base_map + 5, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(15, 0, 5, 1));
    std::fill_n(base_map + 15, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(25, 0, 5, 1));
    std::fill_n(base_map + 25, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(35, 0, 5, 1));
    std::fill_n(base_map + 35, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(45, 0, 5, 1));
    std::fill_n(base_map + 45, 5, true);

    for (size_t i = 0; i < sizeof(kLastRectSizes) / sizeof(kLastRectSizes[0]); i++)
    {
        int last_rect_size = kLastRectSizes[i];
        for (int x = 0; x < kMapWidth - last_rect_size; x++)
        {
            SCOPED_TRACE(x);

            DesktopRegion r = base_region;
            r.addRect(DesktopRect::makeXYWH(x, 0, last_rect_size, 1));

            bool expected_map[kMapWidth];
            std::copy(base_map, base_map + kMapWidth, expected_map);
            std::fill_n(expected_map + x, last_rect_size, true);

            bool map[kMapWidth] = { false };

            int pos = -1;
            for (DesktopRegion::Iterator it(r); !it.isAtEnd(); it.advance())
            {
                EXPECT_GT(it.rect().left(), pos);
                pos = it.rect().right();
                std::fill_n(map + it.rect().left(), it.rect().width(), true);
            }

            EXPECT_TRUE(std::equal(map, map + kMapWidth, expected_map));
        }
    }
}

TEST(desktop_region_test, complex_regions)
{
    struct Case
    {
        int input_count;
        DesktopRect input_rects[4];
        int expected_count;
        DesktopRect expected_rects[6];
    } cases[] =
    {
        {3,
         {
             DesktopRect::makeLTRB(100, 100, 200, 200),
             DesktopRect::makeLTRB(0, 100, 100, 200),
             DesktopRect::makeLTRB(310, 110, 320, 120),
         },
         2,
         {DesktopRect::makeLTRB(0, 100, 200, 200),
          DesktopRect::makeLTRB(310, 110, 320, 120)}},
        {3,
         {DesktopRect::makeLTRB(100, 100, 200, 200),
          DesktopRect::makeLTRB(50, 50, 150, 150),
          DesktopRect::makeLTRB(300, 125, 350, 175)},
         4,
         {DesktopRect::makeLTRB(50, 50, 150, 100),
          DesktopRect::makeLTRB(50, 100, 200, 150),
          DesktopRect::makeLTRB(300, 125, 350, 175),
          DesktopRect::makeLTRB(100, 150, 200, 200)}},
        {4,
         {DesktopRect::makeLTRB(0, 0, 30, 30),
          DesktopRect::makeLTRB(10, 10, 40, 40),
          DesktopRect::makeLTRB(20, 20, 50, 50),
          DesktopRect::makeLTRB(50, 0, 65, 15)},
         6,
         {DesktopRect::makeLTRB(0, 0, 30, 10),
          DesktopRect::makeLTRB(50, 0, 65, 15),
          DesktopRect::makeLTRB(0, 10, 40, 20),
          DesktopRect::makeLTRB(0, 20, 50, 30),
          DesktopRect::makeLTRB(10, 30, 50, 40),
          DesktopRect::makeLTRB(20, 40, 50, 50)}},
        {3,
         {DesktopRect::makeLTRB(10, 10, 40, 20),
          DesktopRect::makeLTRB(10, 30, 40, 40),
          DesktopRect::makeLTRB(10, 20, 40, 30)},
         1,
         {DesktopRect::makeLTRB(10, 10, 40, 40)}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r;
        r.addRects(cases[i].input_rects, cases[i].input_count);
        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);

        // Try inserting rectangles in reverse order.
        r.clear();
        for (int j = cases[i].input_count - 1; j >= 0; --j)
        {
            r.addRect(cases[i].input_rects[j]);
        }
        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);
    }
}

TEST(desktop_region_test, equals)
{
    struct Region
    {
        int count;
        DesktopRect rects[4];
        int id;
    } regions[] = {
        // Same region with one of the rectangles 1 pixel wider/taller.
        {2,
         {DesktopRect::makeLTRB(0, 100, 200, 200),
          DesktopRect::makeLTRB(310, 110, 320, 120)},
         0},
        {2,
         {DesktopRect::makeLTRB(0, 100, 201, 200),
          DesktopRect::makeLTRB(310, 110, 320, 120)},
         1},
        {2,
         {DesktopRect::makeLTRB(0, 100, 200, 201),
          DesktopRect::makeLTRB(310, 110, 320, 120)},
         2},

         // Same region with one of the rectangles shifted horizontally and
         // vertically.
         {4,
          {DesktopRect::makeLTRB(0, 0, 30, 30),
           DesktopRect::makeLTRB(10, 10, 40, 40),
           DesktopRect::makeLTRB(20, 20, 50, 50),
           DesktopRect::makeLTRB(50, 0, 65, 15)},
          3},
         {4,
          {DesktopRect::makeLTRB(0, 0, 30, 30),
           DesktopRect::makeLTRB(10, 10, 40, 40),
           DesktopRect::makeLTRB(20, 20, 50, 50),
           DesktopRect::makeLTRB(50, 1, 65, 16)},
          4},
         {4,
          {DesktopRect::makeLTRB(0, 0, 30, 30),
           DesktopRect::makeLTRB(10, 10, 40, 40),
           DesktopRect::makeLTRB(20, 20, 50, 50),
           DesktopRect::makeLTRB(51, 0, 66, 15)},
          5},

          // Same region defined by a different set of rectangles - one of the
          // rectangle is split horizontally into two.
          {3,
           {DesktopRect::makeLTRB(100, 100, 200, 200),
            DesktopRect::makeLTRB(50, 50, 150, 150),
            DesktopRect::makeLTRB(300, 125, 350, 175)},
           6},
          {4,
           {DesktopRect::makeLTRB(100, 100, 200, 200),
            DesktopRect::makeLTRB(50, 50, 100, 150),
            DesktopRect::makeLTRB(100, 50, 150, 150),
            DesktopRect::makeLTRB(300, 125, 350, 175)},
           6},

           // Rectangle region defined by a set of rectangles that merge into one.
           {3,
            {DesktopRect::makeLTRB(10, 10, 40, 20),
             DesktopRect::makeLTRB(10, 30, 40, 40),
             DesktopRect::makeLTRB(10, 20, 40, 30)},
            7},
           {1, {DesktopRect::makeLTRB(10, 10, 40, 40)}, 7},
    };
    int kTotalRegions = sizeof(regions) / sizeof(Region);

    for (int i = 0; i < kTotalRegions; ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r1(regions[i].rects, regions[i].count);
        for (int j = 0; j < kTotalRegions; ++j)
        {
            SCOPED_TRACE(j);

            DesktopRegion r2(regions[j].rects, regions[j].count);
            EXPECT_EQ(regions[i].id == regions[j].id, r1.equals(r2));
        }
    }
}

TEST(desktop_region_test, translate)
{
    struct Case
    {
        int input_count;
        DesktopRect input_rects[4];
        int dx;
        int dy;
        int expected_count;
        DesktopRect expected_rects[5];
    } cases[] =
    {
        {3,
         {DesktopRect::makeLTRB(0, 0, 30, 30),
          DesktopRect::makeLTRB(10, 10, 40, 40),
          DesktopRect::makeLTRB(20, 20, 50, 50)},
         3,
         5,
         5,
         {DesktopRect::makeLTRB(3, 5, 33, 15),
          DesktopRect::makeLTRB(3, 15, 43, 25),
          DesktopRect::makeLTRB(3, 25, 53, 35),
          DesktopRect::makeLTRB(13, 35, 53, 45),
          DesktopRect::makeLTRB(23, 45, 53, 55)}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r(cases[i].input_rects, cases[i].input_count);
        r.translate(cases[i].dx, cases[i].dy);
        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);
    }
}

TEST(desktop_region_test, intersect)
{
    struct Case
    {
        int input1_count;
        DesktopRect input1_rects[4];
        int input2_count;
        DesktopRect input2_rects[4];
        int expected_count;
        DesktopRect expected_rects[5];
    } cases[] =
    {
        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(50, 50, 150, 150)},
         1,
         {DesktopRect::makeLTRB(50, 50, 100, 100)}},

        {1,
         {DesktopRect::makeLTRB(100, 0, 200, 300)},
         1,
         {DesktopRect::makeLTRB(0, 100, 300, 200)},
         1,
         {DesktopRect::makeLTRB(100, 100, 200, 200)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         2,
         {DesktopRect::makeLTRB(50, 10, 150, 30),
          DesktopRect::makeLTRB(50, 30, 160, 50)},
         1,
         {DesktopRect::makeLTRB(50, 10, 100, 50)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         2,
         {DesktopRect::makeLTRB(50, 10, 150, 30),
          DesktopRect::makeLTRB(50, 30, 90, 50)},
         2,
         {DesktopRect::makeLTRB(50, 10, 100, 30),
          DesktopRect::makeLTRB(50, 30, 90, 50)}},
        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(100, 50, 200, 200)},
         0,
         {}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r1(cases[i].input1_rects, cases[i].input1_count);
        DesktopRegion r2(cases[i].input2_rects, cases[i].input2_count);

        DesktopRegion r;
        r.intersect(r1, r2);

        compareRegion(r, cases[i].expected_rects, cases[i].expected_count);
    }
}

TEST(desktop_region_test, subtract)
{
    struct Case
    {
        int input1_count;
        DesktopRect input1_rects[4];
        int input2_count;
        DesktopRect input2_rects[4];
        int expected_count;
        DesktopRect expected_rects[5];
    } cases[] =
    {
        // Subtract one rect from another.
        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(50, 50, 150, 150)},
         2,
         {DesktopRect::makeLTRB(0, 0, 100, 50),
          DesktopRect::makeLTRB(0, 50, 50, 100)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(-50, -50, 50, 50)},
         2,
         {DesktopRect::makeLTRB(50, 0, 100, 50),
          DesktopRect::makeLTRB(0, 50, 100, 100)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(-50, 50, 50, 150)},
         2,
         {DesktopRect::makeLTRB(0, 0, 100, 50),
          DesktopRect::makeLTRB(50, 50, 100, 100)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(50, 50, 150, 70)},
         3,
         {DesktopRect::makeLTRB(0, 0, 100, 50),
          DesktopRect::makeLTRB(0, 50, 50, 70),
          DesktopRect::makeLTRB(0, 70, 100, 100)}},

        {1,
         {DesktopRect::makeLTRB(0, 0, 100, 100)},
         1,
         {DesktopRect::makeLTRB(50, 50, 70, 70)},
         4,
         {DesktopRect::makeLTRB(0, 0, 100, 50),
          DesktopRect::makeLTRB(0, 50, 50, 70),
          DesktopRect::makeLTRB(70, 50, 100, 70),
          DesktopRect::makeLTRB(0, 70, 100, 100)}},

          // Empty result.
          {1,
           {DesktopRect::makeLTRB(0, 0, 100, 100)},
           1,
           {DesktopRect::makeLTRB(0, 0, 100, 100)},
           0,
           {}},

          {1,
           {DesktopRect::makeLTRB(0, 0, 100, 100)},
           1,
           {DesktopRect::makeLTRB(-10, -10, 110, 110)},
           0,
           {}},

          {2,
           {DesktopRect::makeLTRB(0, 0, 100, 100),
            DesktopRect::makeLTRB(50, 50, 150, 150)},
           2,
           {DesktopRect::makeLTRB(0, 0, 100, 100),
            DesktopRect::makeLTRB(50, 50, 150, 150)},
           0,
           {}},

           // One rect out of disjoint set.
           {3,
            {DesktopRect::makeLTRB(0, 0, 10, 10),
             DesktopRect::makeLTRB(20, 20, 30, 30),
             DesktopRect::makeLTRB(40, 0, 50, 10)},
            1,
            {DesktopRect::makeLTRB(20, 20, 30, 30)},
            2,
            {DesktopRect::makeLTRB(0, 0, 10, 10),
             DesktopRect::makeLTRB(40, 0, 50, 10)}},

             // Row merging.
             {3,
              {DesktopRect::makeLTRB(0, 0, 100, 50),
               DesktopRect::makeLTRB(0, 50, 150, 70),
               DesktopRect::makeLTRB(0, 70, 100, 100)},
              1,
              {DesktopRect::makeLTRB(100, 50, 150, 70)},
              1,
              {DesktopRect::makeLTRB(0, 0, 100, 100)}},

              // No-op subtraction.
              {1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)},
               1,
               {DesktopRect::makeLTRB(100, 0, 200, 100)},
               1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)}},

              {1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)},
               1,
               {DesktopRect::makeLTRB(-100, 0, 0, 100)},
               1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)}},

              {1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)},
               1,
               {DesktopRect::makeLTRB(0, 100, 0, 200)},
               1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)}},

              {1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)},
               1,
               {DesktopRect::makeLTRB(0, -100, 100, 0)},
               1,
               {DesktopRect::makeLTRB(0, 0, 100, 100)}},
    };

    for (size_t i = 0; i < (sizeof(cases) / sizeof(Case)); ++i)
    {
        SCOPED_TRACE(i);

        DesktopRegion r1(cases[i].input1_rects, cases[i].input1_count);
        DesktopRegion r2(cases[i].input2_rects, cases[i].input2_count);

        r1.subtract(r2);

        compareRegion(r1, cases[i].expected_rects, cases[i].expected_count);
    }
}

// Verify that DesktopRegion::SubtractRows() works correctly by creating a row
// of not overlapping rectangles and subtracting a set of rectangle. Result
// is verified by building a map of the region in an array and comparing it with
// the expected values.
TEST(desktop_region_test, subtract_rect_on_same_row)
{
    const int kMapWidth = 50;

    struct SpanSet
    {
        int count;
        struct Range
        {
            int start;
            int end;
        } spans[3];
    } span_sets[] =
    {
        {1, {{0, 3}}},
        {1, {{0, 5}}},
        {1, {{0, 7}}},
        {1, {{0, 12}}},
        {2, {{0, 3}, {4, 5}, {6, 16}}},
    };

    DesktopRegion base_region;
    bool base_map[kMapWidth] = { false };

    base_region.addRect(DesktopRect::makeXYWH(5, 0, 5, 1));
    std::fill_n(base_map + 5, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(15, 0, 5, 1));
    std::fill_n(base_map + 15, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(25, 0, 5, 1));
    std::fill_n(base_map + 25, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(35, 0, 5, 1));
    std::fill_n(base_map + 35, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(45, 0, 5, 1));
    std::fill_n(base_map + 45, 5, true);

    for (size_t i = 0; i < sizeof(span_sets) / sizeof(span_sets[0]); i++)
    {
        SCOPED_TRACE(i);
        SpanSet& span_set = span_sets[i];
        int span_set_end = span_set.spans[span_set.count - 1].end;
        for (int x = 0; x < kMapWidth - span_set_end; ++x)
        {
            SCOPED_TRACE(x);

            DesktopRegion r = base_region;

            bool expected_map[kMapWidth];
            std::copy(base_map, base_map + kMapWidth, expected_map);

            DesktopRegion region2;
            for (int span = 0; span < span_set.count; span++)
            {
                std::fill_n(x + expected_map + span_set.spans[span].start,
                            span_set.spans[span].end - span_set.spans[span].start,
                            false);
                region2.addRect(DesktopRect::makeLTRB(x + span_set.spans[span].start, 0,
                                                      x + span_set.spans[span].end, 1));
            }
            r.subtract(region2);

            bool map[kMapWidth] = { false };

            int pos = -1;
            for (DesktopRegion::Iterator it(r); !it.isAtEnd(); it.advance())
            {
                EXPECT_GT(it.rect().left(), pos);
                pos = it.rect().right();
                std::fill_n(map + it.rect().left(), it.rect().width(), true);
            }

            EXPECT_TRUE(std::equal(map, map + kMapWidth, expected_map));
        }
    }
}

// Verify that DesktopRegion::Subtract() works correctly by creating a column of
// not overlapping rectangles and subtracting a set of rectangle on the same
// column. Result is verified by building a map of the region in an array and
// comparing it with the expected values.
TEST(desktop_region_test, subtract_rect_on_same_col)
{
    const int kMapHeight = 50;

    struct SpanSet
    {
        int count;
        struct Range
        {
            int start;
            int end;
        } spans[3];
    } span_sets[] =
    {
        {1, {{0, 3}}},
        {1, {{0, 5}}},
        {1, {{0, 7}}},
        {1, {{0, 12}}},
        {2, {{0, 3}, {4, 5}, {6, 16}}},
    };

    DesktopRegion base_region;
    bool base_map[kMapHeight] = { false };

    base_region.addRect(DesktopRect::makeXYWH(0, 5, 1, 5));
    std::fill_n(base_map + 5, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(0, 15, 1, 5));
    std::fill_n(base_map + 15, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(0, 25, 1, 5));
    std::fill_n(base_map + 25, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(0, 35, 1, 5));
    std::fill_n(base_map + 35, 5, true);
    base_region.addRect(DesktopRect::makeXYWH(0, 45, 1, 5));
    std::fill_n(base_map + 45, 5, true);

    for (size_t i = 0; i < sizeof(span_sets) / sizeof(span_sets[0]); i++)
    {
        SCOPED_TRACE(i);
        SpanSet& span_set = span_sets[i];
        int span_set_end = span_set.spans[span_set.count - 1].end;
        for (int y = 0; y < kMapHeight - span_set_end; ++y)
        {
            SCOPED_TRACE(y);

            DesktopRegion r = base_region;

            bool expected_map[kMapHeight];
            std::copy(base_map, base_map + kMapHeight, expected_map);

            DesktopRegion region2;
            for (int span = 0; span < span_set.count; span++)
            {
                std::fill_n(y + expected_map + span_set.spans[span].start,
                            span_set.spans[span].end - span_set.spans[span].start,
                            false);
                region2.addRect(DesktopRect::makeLTRB(0, y + span_set.spans[span].start,
                                                      1, y + span_set.spans[span].end));
            }
            r.subtract(region2);

            bool map[kMapHeight] = { false };

            int pos = -1;
            for (DesktopRegion::Iterator it(r); !it.isAtEnd(); it.advance())
            {
                EXPECT_GT(it.rect().top(), pos);
                pos = it.rect().bottom();
                std::fill_n(map + it.rect().top(), it.rect().height(), true);
            }

            for (int j = 0; j < kMapHeight; j++)
            {
                EXPECT_EQ(expected_map[j], map[j]) << "j = " << j;
            }
        }
    }
}

TEST(desktop_region_test, performance)
{
    for (int c = 0; c < 1000; ++c)
    {
        DesktopRegion r;
        for (int i = 0; i < 10; ++i)
        {
            r.addRect(DesktopRect::makeXYWH(radmonInt(1000), radmonInt(1000), 200, 200));
        }

        for (int i = 0; i < 1000; ++i)
        {
            r.addRect(DesktopRect::makeXYWH(radmonInt(1000), radmonInt(1000),
                                            5 + radmonInt(10) * 5,
                                            5 + radmonInt(10) * 5));
        }

        // Iterate over the rectangles.
        for (DesktopRegion::Iterator it(r); !it.isAtEnd(); it.advance())
        {
        }
    }
}

}  // namespace webrtc
