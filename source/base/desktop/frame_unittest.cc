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

#include "base/desktop/frame_simple.h"

#include <gtest/gtest.h>

namespace base {

namespace {

std::unique_ptr<Frame> createTestFrame(const QRect& rect, int pixels_value)
{
    QSize size = rect.size();
    auto frame = FrameSimple::create(size, PixelFormat::ARGB());
    frame->setTopLeft(rect.topLeft());
    memset(frame->frameData(),
           pixels_value,
           static_cast<size_t>(frame->stride()) * static_cast<size_t>(size.height()));
    return std::move(frame);
}

} // namespace

TEST(FrameTest, Performance)
{
    QRect frame_rect(QPoint(0, 0), QSize(1024, 768));
    auto frame1 = createTestFrame(frame_rect, 0);
    auto frame2 = createTestFrame(frame_rect, 0xff);

    struct
    {
        QRect src_rect;
        QRect dst_rect;
    } cases[] =
    {
        { QRect(QPoint(0, 0), QSize(120, 175)), QRect(QPoint(50, 50), QSize(120, 175)) },
        { QRect(QPoint(100, 200), QSize(50, 100)), QRect(QPoint(500, 400), QSize(50, 100)) },
        { QRect(QPoint(75, 60), QSize(10, 5)), QRect(QPoint(30, 45), QSize(10, 5)) },
        { QRect(QPoint(500, 200), QSize(200, 200)), QRect(QPoint(200, 500), QSize(200, 200)) },
        { QRect(QPoint(350, 0), QSize(100, 50)), QRect(QPoint(0, 350), QSize(100, 50)) }
    };

    for (size_t n = 0; n < 100000; ++n)
    {
        for (size_t i = 0; i < std::size(cases); ++i)
        {
            frame1->copyPixelsFrom(*frame2, cases[i].src_rect.topLeft(), cases[i].dst_rect);
        }

        for (size_t i = 0; i < std::size(cases); ++i)
        {
            frame2->copyPixelsFrom(*frame1, cases[i].src_rect.topLeft(), cases[i].dst_rect);
        }
    }
}

} // namespace base
