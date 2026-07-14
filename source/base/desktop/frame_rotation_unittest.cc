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

#include "base/desktop/frame_rotation.h"

#include "base/desktop/frame.h"
#include "base/desktop/frame_aligned.h"

#include <gtest/gtest.h>

#include <QPoint>
#include <QRect>
#include <QSize>

#include <memory>
#include <vector>

namespace {

const Rotation kAllRotations[] = {
    Rotation::CLOCK_WISE_0,
    Rotation::CLOCK_WISE_90,
    Rotation::CLOCK_WISE_180,
    Rotation::CLOCK_WISE_270,
};

//--------------------------------------------------------------------------------------------------
// Encodes the coordinates of a pixel into a unique ARGB value so the pixel can be tracked across a
// rotation. Frames used by the tests stay well below 256x256, so 8 bits per axis is enough.
quint32 encodePixel(int x, int y)
{
    return 0xFF000000u | (static_cast<quint32>(x) << 8) | static_cast<quint32>(y);
}

//--------------------------------------------------------------------------------------------------
quint32 pixelAt(const Frame& frame, int x, int y)
{
    return *reinterpret_cast<const quint32*>(frame.frameDataAtPos(x, y));
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<FrameAligned> makeGradientFrame(const QSize& size)
{
    std::unique_ptr<FrameAligned> frame = FrameAligned::create(size, 32);

    for (int y = 0; y < size.height(); ++y)
    {
        for (int x = 0; x < size.width(); ++x)
            *reinterpret_cast<quint32*>(frame->frameDataAtPos(x, y)) = encodePixel(x, y);
    }

    return frame;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(FrameRotationTest, RotateSize)
{
    const QSize size(200, 100);

    EXPECT_EQ(rotateSize(size, Rotation::CLOCK_WISE_0), QSize(200, 100));
    EXPECT_EQ(rotateSize(size, Rotation::CLOCK_WISE_90), QSize(100, 200));
    EXPECT_EQ(rotateSize(size, Rotation::CLOCK_WISE_180), QSize(200, 100));
    EXPECT_EQ(rotateSize(size, Rotation::CLOCK_WISE_270), QSize(100, 200));
}

//--------------------------------------------------------------------------------------------------
TEST(FrameRotationTest, ReverseRotation)
{
    EXPECT_EQ(reverseRotation(Rotation::CLOCK_WISE_0), Rotation::CLOCK_WISE_0);
    EXPECT_EQ(reverseRotation(Rotation::CLOCK_WISE_90), Rotation::CLOCK_WISE_270);
    EXPECT_EQ(reverseRotation(Rotation::CLOCK_WISE_180), Rotation::CLOCK_WISE_180);
    EXPECT_EQ(reverseRotation(Rotation::CLOCK_WISE_270), Rotation::CLOCK_WISE_90);
}

//--------------------------------------------------------------------------------------------------
TEST(FrameRotationTest, RotateRectIdentity)
{
    const QRect rect(10, 20, 30, 40);
    EXPECT_EQ(rotateRect(rect, QSize(100, 100), Rotation::CLOCK_WISE_0), rect);
}

//--------------------------------------------------------------------------------------------------
// Expected values are derived geometrically (independently of the implementation) from the pixel
// mapping of a clockwise frame rotation: for a WxH frame, CW90 maps (x,y)->(H-1-y,x),
// CW180 -> (W-1-x,H-1-y), CW270 -> (y,W-1-x). A rect at the top-left corner of a 200x100 frame is
// used because a one-pixel edge error (the bug this suite guards against) is easy to spot there.
TEST(FrameRotationTest, RotateRectKnownGeometry)
{
    const QSize size(200, 100); // W=200, H=100.
    const QRect rect(0, 0, 10, 5);

    EXPECT_EQ(rotateRect(rect, size, Rotation::CLOCK_WISE_90), QRect(95, 0, 5, 10));
    EXPECT_EQ(rotateRect(rect, size, Rotation::CLOCK_WISE_180), QRect(190, 95, 10, 5));
    EXPECT_EQ(rotateRect(rect, size, Rotation::CLOCK_WISE_270), QRect(0, 190, 5, 10));
}

//--------------------------------------------------------------------------------------------------
// Regression test for the inclusive/exclusive QRect edge off-by-one: before the fix this returned
// (61,41,30,40) instead of (60,40,30,40).
TEST(FrameRotationTest, RotateRect180OffByOneRegression)
{
    EXPECT_EQ(rotateRect(QRect(10, 20, 30, 40), QSize(100, 100), Rotation::CLOCK_WISE_180),
              QRect(60, 40, 30, 40));
}

//--------------------------------------------------------------------------------------------------
// A full-frame rect must rotate to the full rotated frame anchored at (0,0). Before the fix it was
// shifted to (1,1,...) and spilled one row/column past the frame, which is what caused the
// out-of-bounds pixel read in the rotated-display capture path.
TEST(FrameRotationTest, RotateRectFullFrameStaysInBounds)
{
    const QSize size(200, 100);
    const QRect full(QPoint(0, 0), size);

    for (Rotation rotation : kAllRotations)
    {
        const QSize rotated_size = rotateSize(size, rotation);
        const QRect result = rotateRect(full, size, rotation);

        EXPECT_EQ(result, QRect(QPoint(0, 0), rotated_size)) << "rotation=" << static_cast<int>(rotation);
        EXPECT_TRUE(QRect(QPoint(0, 0), rotated_size).contains(result))
            << "rotation=" << static_cast<int>(rotation);
    }
}

//--------------------------------------------------------------------------------------------------
// Rotating a rect and then applying the reverse rotation must reproduce the original rect exactly.
TEST(FrameRotationTest, RotateRectRoundTrip)
{
    const QSize size(200, 100);
    const std::vector<QRect> rects = {
        QRect(0, 0, 200, 100),   // Whole frame.
        QRect(10, 20, 30, 40),   // Interior.
        QRect(0, 0, 1, 1),       // Top-left pixel.
        QRect(199, 99, 1, 1),    // Bottom-right pixel.
        QRect(0, 99, 200, 1),    // Bottom row.
        QRect(199, 0, 1, 100),   // Right column.
    };

    for (Rotation rotation : kAllRotations)
    {
        for (const QRect& rect : rects)
        {
            const QRect rotated = rotateRect(rect, size, rotation);
            const QRect restored =
                rotateRect(rotated, rotateSize(size, rotation), reverseRotation(rotation));

            EXPECT_EQ(restored, rect)
                << "rotation=" << static_cast<int>(rotation) << " rect=" << rect.x() << ","
                << rect.y() << "," << rect.width() << "," << rect.height();
        }
    }
}

//--------------------------------------------------------------------------------------------------
// End-to-end check through rotateFrame(): rotating a whole frame and then rotating the result back
// must reproduce every source pixel. This exercises the rect math used for both the source and
// target rectangles, so a one-pixel edge error would misplace pixels and fail the comparison.
TEST(FrameRotationTest, RotateFrameRoundTripPixels)
{
    const QSize size(7, 5);

    for (Rotation rotation : kAllRotations)
    {
        std::unique_ptr<FrameAligned> source = makeGradientFrame(size);

        const QSize rotated_size = rotateSize(size, rotation);
        std::unique_ptr<FrameAligned> rotated = FrameAligned::create(rotated_size, 32);
        rotateFrame(*source, QRect(QPoint(0, 0), size), rotation, QPoint(0, 0), rotated.get());

        std::unique_ptr<FrameAligned> restored = FrameAligned::create(size, 32);
        rotateFrame(*rotated, QRect(QPoint(0, 0), rotated_size), reverseRotation(rotation),
                    QPoint(0, 0), restored.get());

        for (int y = 0; y < size.height(); ++y)
        {
            for (int x = 0; x < size.width(); ++x)
            {
                EXPECT_EQ(pixelAt(*restored, x, y), encodePixel(x, y))
                    << "rotation=" << static_cast<int>(rotation) << " x=" << x << " y=" << y;
            }
        }
    }
}
