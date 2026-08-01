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

#include "base/desktop/frame_aligned.h"

#include <gtest/gtest.h>

namespace {

std::unique_ptr<Frame> createTestFrame(const QRect& rect, int pixels_value)
{
    QSize size = rect.size();
    auto frame = FrameAligned::create(size, 32);
    frame->setTopLeft(rect.topLeft());
    memset(frame->frameData(),
           pixels_value,
           static_cast<size_t>(frame->stride()) * static_cast<size_t>(size.height()));
    return frame;
}

} // namespace

TEST(FrameTest, InvalidSize)
{
    EXPECT_EQ(FrameAligned::create(QSize(0, 100), 32), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(100, 0), 32), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(-1, 100), 32), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(100, -1), 32), nullptr);

    // The row does not fit into the int stride.
    EXPECT_EQ(FrameAligned::create(QSize(600 * 1000 * 1000, 1), 32), nullptr);
}

TEST(FrameTest, InvalidAlignment)
{
    // Rounding the stride up is a power-of-two mask: any other value gives a stride that does not
    // fit the row.
    EXPECT_EQ(FrameAligned::create(QSize(100, 100), 0), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(100, 100), 3), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(100, 100), 24), nullptr);
    EXPECT_EQ(FrameAligned::create(QSize(100, 100), 48), nullptr);
}

TEST(FrameTest, NewFrameIsZeroed)
{
    const QSize size(100, 8);
    std::unique_ptr<FrameAligned> frame = FrameAligned::create(size, 32);
    ASSERT_NE(frame, nullptr);

    EXPECT_EQ(frame->stride() % 32, 0);
    EXPECT_GE(frame->stride(), size.width() * Frame::kBytesPerPixel);

    const size_t buffer_size = static_cast<size_t>(frame->stride()) *
                               static_cast<size_t>(size.height());
    const quint8* data = frame->frameData();

    for (size_t i = 0; i < buffer_size; ++i)
        ASSERT_EQ(data[i], 0) << "Byte " << i << " is not zeroed";
}

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
