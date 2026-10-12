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
TEST(CursorCodecTest, EncoderSetsTimestamp)
{
    CursorEncoder encoder;
    const MouseCursor cursor = makeCursor(16, 16, 3);

    proto::cursor::Shape first;
    ASSERT_TRUE(encoder.encode(cursor, &first));
    EXPECT_NE(first.timestamp(), 0u);

    proto::cursor::Shape second;
    ASSERT_TRUE(encoder.encode(cursor, &second));
    EXPECT_GE(second.timestamp(), first.timestamp());
}

//--------------------------------------------------------------------------------------------------
// The host recreates the encoder when the network channel is switched, and shapes of the previous
// encoder sent over the old channel can arrive after the cache reset of the new one.
TEST(CursorCodecTest, DecoderDropsShapesEncodedBeforeReset)
{
    const MouseCursor old_first = makeCursor(16, 16, 4);
    const MouseCursor old_second = makeCursor(24, 24, 5);
    const MouseCursor current = makeCursor(32, 32, 6);

    CursorEncoder old_encoder;
    proto::cursor::Shape old_reset;
    proto::cursor::Shape old_image;
    proto::cursor::Shape old_reference;
    ASSERT_TRUE(old_encoder.encode(old_first, &old_reset));
    ASSERT_TRUE(old_encoder.encode(old_second, &old_image));
    ASSERT_TRUE(old_encoder.encode(old_first, &old_reference));
    ASSERT_TRUE(old_reference.flags() & proto::cursor::Shape::CACHE);
    old_reset.set_timestamp(100);
    old_image.set_timestamp(200);
    old_reference.set_timestamp(300);

    CursorEncoder new_encoder;
    proto::cursor::Shape new_reset;
    proto::cursor::Shape new_reference;
    ASSERT_TRUE(new_encoder.encode(current, &new_reset));
    ASSERT_TRUE(new_encoder.encode(current, &new_reference));
    ASSERT_TRUE(new_reference.flags() & proto::cursor::Shape::CACHE);
    new_reset.set_timestamp(400);
    new_reference.set_timestamp(500);

    CursorDecoder decoder;
    ASSERT_NE(decoder.decode(old_reset), nullptr);
    ASSERT_NE(decoder.decode(new_reset), nullptr);

    EXPECT_EQ(decoder.decode(old_image), nullptr);
    EXPECT_EQ(decoder.decode(old_reference), nullptr);
    EXPECT_EQ(decoder.decode(old_reset), nullptr);
    EXPECT_EQ(decoder.cachedCursors(), 1);

    const std::shared_ptr<MouseCursor> cached = decoder.decode(new_reference);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->constImage(), current.constImage());
}

//--------------------------------------------------------------------------------------------------
// Hosts up to 3.0.33 and 2.x do not send a timestamp.
TEST(CursorCodecTest, DecoderAcceptsShapesWithoutTimestamp)
{
    CursorEncoder encoder;
    CursorDecoder decoder;

    const MouseCursor first_cursor = makeCursor(16, 16, 7);
    const MouseCursor second_cursor = makeCursor(24, 24, 8);

    proto::cursor::Shape first;
    proto::cursor::Shape second;
    proto::cursor::Shape reference;
    ASSERT_TRUE(encoder.encode(first_cursor, &first));
    ASSERT_TRUE(encoder.encode(second_cursor, &second));
    ASSERT_TRUE(encoder.encode(first_cursor, &reference));
    first.clear_timestamp();
    first.set_flags(proto::cursor::Shape::RESET_CACHE | 30);
    second.clear_timestamp();
    reference.clear_timestamp();

    ASSERT_NE(decoder.decode(first), nullptr);
    ASSERT_NE(decoder.decode(second), nullptr);

    const std::shared_ptr<MouseCursor> cached = decoder.decode(reference);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->constImage(), first_cursor.constImage());
}

//--------------------------------------------------------------------------------------------------
TEST(CursorCodecTest, CacheIndexAbove31)
{
    CursorEncoder encoder;
    CursorDecoder decoder;

    for (int i = 0; i < 40; ++i)
    {
        proto::cursor::Shape shape;
        ASSERT_TRUE(encoder.encode(makeCursor(8, 8, i), &shape));
        if (i == 0)
            EXPECT_EQ(shape.flags(), proto::cursor::Shape::RESET_CACHE | 63u);
        ASSERT_NE(decoder.decode(shape), nullptr);
    }
    EXPECT_EQ(decoder.cachedCursors(), 40);

    const MouseCursor cursor = makeCursor(8, 8, 35);
    proto::cursor::Shape reference;
    ASSERT_TRUE(encoder.encode(cursor, &reference));
    EXPECT_EQ(reference.flags(), proto::cursor::Shape::CACHE | 35u);

    const std::shared_ptr<MouseCursor> cached = decoder.decode(reference);
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->constImage(), cursor.constImage());
}

//--------------------------------------------------------------------------------------------------
// Hosts up to 3.0.33 and 2.x use 5 bits for the cache size, so the limit stays 30 for them.
TEST(CursorCodecTest, DecoderLimitsCacheSizeWithoutTimestamp)
{
    CursorEncoder encoder;
    CursorDecoder decoder;

    proto::cursor::Shape shape;
    ASSERT_TRUE(encoder.encode(makeCursor(16, 16, 9), &shape));
    shape.clear_timestamp();

    shape.set_flags(proto::cursor::Shape::RESET_CACHE | 31);
    EXPECT_EQ(decoder.decode(shape), nullptr);

    shape.set_flags(proto::cursor::Shape::RESET_CACHE | 30);
    EXPECT_NE(decoder.decode(shape), nullptr);
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
