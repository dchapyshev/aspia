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

#include "host/input_injector.h"

#include <gtest/gtest.h>

#include <limits>

//--------------------------------------------------------------------------------------------------
TEST(input_injector_test, map_to_screen_same_size)
{
    const QSize size(1920, 1080);

    EXPECT_EQ(InputInjector::mapToScreen(QPoint(0, 0), size, size), QPoint(0, 0));
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(800, 450), size, size), QPoint(800, 450));
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(1919, 1079), size, size), QPoint(1919, 1079));
}

//--------------------------------------------------------------------------------------------------
TEST(input_injector_test, map_to_screen_reaches_last_pixel)
{
    const QSize screen(1920, 1080);

    EXPECT_EQ(InputInjector::mapToScreen(QPoint(0, 0), QSize(960, 540), screen), QPoint(0, 0));
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(959, 539), QSize(960, 540), screen),
              QPoint(1919, 1079));
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(479, 269), QSize(480, 270), screen),
              QPoint(1919, 1079));
}

//--------------------------------------------------------------------------------------------------
TEST(input_injector_test, map_to_screen_axes_are_independent)
{
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(99, 99), QSize(100, 200), QSize(200, 200)),
              QPoint(199, 99));
}

//--------------------------------------------------------------------------------------------------
TEST(input_injector_test, map_to_screen_single_pixel_frame)
{
    EXPECT_EQ(InputInjector::mapToScreen(QPoint(0, 0), QSize(1, 1), QSize(1920, 1080)), QPoint(0, 0));
}

//--------------------------------------------------------------------------------------------------
TEST(input_injector_test, map_to_screen_untrusted_position)
{
    const int min = std::numeric_limits<int>::min();
    const int max = std::numeric_limits<int>::max();

    const QPoint result = InputInjector::mapToScreen(
        QPoint(max, min), QSize(2, 2), QSize(1920, 1080));

    EXPECT_EQ(result, QPoint(max, min));
}
