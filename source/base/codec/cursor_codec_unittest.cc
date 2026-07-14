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

#include "base/codec/cursor_decoder.h"
#include "base/codec/cursor_encoder.h"

#include "base/desktop/mouse_cursor.h"
#include "proto/desktop_cursor.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QPoint>
#include <QSize>

#include <memory>

namespace {

//--------------------------------------------------------------------------------------------------
// Builds a cursor with a deterministic ARGB image so it can be compared after a round-trip.
MouseCursor makeCursor(int width, int height, int seed)
{
    QByteArray image(width * height * MouseCursor::kBytesPerPixel, 0);
    for (int i = 0; i < image.size(); ++i)
        image[i] = static_cast<char>((i * 13 + seed * 7) & 0xFF);

    return MouseCursor(std::move(image), QSize(width, height),
                       QPoint(width / 2, height / 2), QPoint(96, 96));
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, EncodeDecodeRoundTrip)
{
    CursorEncoder encoder;
    CursorDecoder decoder;

    const MouseCursor original = makeCursor(24, 32, 1);

    proto::cursor::Shape shape;
    ASSERT_TRUE(encoder.encode(original, &shape));
    EXPECT_EQ(shape.width(), 24);
    EXPECT_EQ(shape.height(), 32);
    EXPECT_FALSE(shape.data().empty());

    const std::shared_ptr<MouseCursor> decoded = decoder.decode(shape);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->size(), original.size());
    EXPECT_EQ(decoded->hotSpot(), original.hotSpot());
    EXPECT_EQ(decoded->constDpi(), QPoint(96, 96));
    EXPECT_EQ(decoded->constImage(), original.constImage());
}

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, RepeatedCursorUsesCache)
{
    CursorEncoder encoder;
    CursorDecoder decoder;

    const MouseCursor cursor = makeCursor(16, 16, 2);

    proto::cursor::Shape first;
    ASSERT_TRUE(encoder.encode(cursor, &first));
    EXPECT_TRUE(first.flags() & proto::cursor::Shape::RESET_CACHE);
    EXPECT_FALSE(first.data().empty());

    proto::cursor::Shape second;
    ASSERT_TRUE(encoder.encode(cursor, &second));
    EXPECT_TRUE(second.flags() & proto::cursor::Shape::CACHE);
    EXPECT_TRUE(second.data().empty());

    ASSERT_NE(decoder.decode(first), nullptr);
    const std::shared_ptr<MouseCursor> cached = decoder.decode(second);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->constImage(), cursor.constImage());
}

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, EncoderRejectsInvalidSize)
{
    CursorEncoder encoder;
    proto::cursor::Shape shape;

    const MouseCursor empty; // Default: 0x0.
    EXPECT_FALSE(encoder.encode(empty, &shape));

    const MouseCursor oversized = makeCursor(513, 1, 0); // Exceeds the 512 limit.
    EXPECT_FALSE(encoder.encode(oversized, &shape));
}

//--------------------------------------------------------------------------------------------------
// The decoder consumes shapes from a potentially malicious host; the tests below feed it hand-built
// shapes and require it to reject them rather than crash, hang, or read out of bounds.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, DecoderRejectsCacheReferenceWithoutReset)
{
    CursorDecoder decoder;

    proto::cursor::Shape shape;
    shape.set_flags(proto::cursor::Shape::CACHE | 0); // Reference the cache before any reset.
    EXPECT_EQ(decoder.decode(shape), nullptr);
}

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, DecoderRejectsInvalidSize)
{
    CursorDecoder decoder;

    proto::cursor::Shape zero;
    zero.set_width(0);
    zero.set_height(10);
    zero.set_data(std::string(16, '\0'));
    EXPECT_EQ(decoder.decode(zero), nullptr);

    proto::cursor::Shape huge;
    huge.set_width(10000);
    huge.set_height(10);
    huge.set_data(std::string(16, '\0'));
    EXPECT_EQ(decoder.decode(huge), nullptr);
}

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, DecoderRejectsEmptyData)
{
    CursorDecoder decoder;

    proto::cursor::Shape shape;
    shape.set_width(16);
    shape.set_height(16);
    shape.set_flags(proto::cursor::Shape::RESET_CACHE | 30);
    // No compressed data at all.
    EXPECT_EQ(decoder.decode(shape), nullptr);
}

//--------------------------------------------------------------------------------------------------
// Regression test for the zstd decompress loop: a shape that declares a tiny 1x1 cursor (4-byte
// image buffer) but carries a zstd stream that decompresses to far more must be rejected promptly,
// not spin forever. A valid stream for a large cursor is produced with the encoder.
TEST(CursorCodecTest, DecoderRejectsOversizedZstdWithoutHanging)
{
    CursorEncoder encoder;
    proto::cursor::Shape big;
    ASSERT_TRUE(encoder.encode(makeCursor(64, 64, 9), &big));
    ASSERT_FALSE(big.data().empty());

    CursorDecoder decoder;
    proto::cursor::Shape malicious;
    malicious.set_width(1);
    malicious.set_height(1);
    malicious.set_flags(proto::cursor::Shape::RESET_CACHE | 30);
    malicious.set_data(big.data()); // Expands to 64 * 64 * 4 bytes, far past the 1x1 buffer.

    EXPECT_EQ(decoder.decode(malicious), nullptr);
}
