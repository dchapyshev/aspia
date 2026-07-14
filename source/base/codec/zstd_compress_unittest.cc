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

#include "base/codec/zstd_compress.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QtEndian>

#include <cstring>
#include <string>

namespace {

//--------------------------------------------------------------------------------------------------
// Builds a deterministic, effectively incompressible byte buffer (an LCG stream) of the given size,
// so that truncating half the compressed data reliably loses part of the content.
QByteArray makeData(int size, int seed)
{
    QByteArray data(size, 0);
    quint32 state = static_cast<quint32>(seed) * 2654435761u + 1u;
    for (int i = 0; i < size; ++i)
    {
        state = state * 1664525u + 1013904223u;
        data[i] = static_cast<char>(state >> 24);
    }
    return data;
}

} // namespace

//--------------------------------------------------------------------------------------------------
TEST(ZstdCompressTest, RoundTripQByteArray)
{
    for (int size : { 1, 2, 100, 4096, 100000 })
    {
        const QByteArray original = makeData(size, size);
        const QByteArray compressed = ZstdCompress::compress(original);
        ASSERT_FALSE(compressed.isEmpty()) << "size=" << size;
        EXPECT_EQ(ZstdCompress::decompress(compressed), original) << "size=" << size;
    }
}

//--------------------------------------------------------------------------------------------------
TEST(ZstdCompressTest, RoundTripStdString)
{
    const std::string original = "The quick brown fox jumps over the lazy dog. 1234567890.";
    const std::string compressed = ZstdCompress::compress(original);
    ASSERT_FALSE(compressed.empty());
    EXPECT_EQ(ZstdCompress::decompress(compressed), original);
}

//--------------------------------------------------------------------------------------------------
TEST(ZstdCompressTest, DecompressRejectsTooShort)
{
    EXPECT_TRUE(ZstdCompress::decompress(QByteArray()).isEmpty());
    EXPECT_TRUE(ZstdCompress::decompress(QByteArray(4, 0)).isEmpty()); // Only the length prefix.
}

//--------------------------------------------------------------------------------------------------
TEST(ZstdCompressTest, DecompressRejectsCorruptPayload)
{
    QByteArray corrupt = ZstdCompress::compress(makeData(1000, 3));
    ASSERT_GT(corrupt.size(), 8);

    // Invert the zstd frame body (everything after the 4-byte length prefix), which destroys the
    // frame magic number and makes decompression fail.
    for (int i = 4; i < corrupt.size(); ++i)
        corrupt[i] = static_cast<char>(~corrupt[i]);

    EXPECT_TRUE(ZstdCompress::decompress(corrupt).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// A truncated compressed stream cannot produce the declared number of bytes, so decompress() must
// reject it instead of returning a partially-filled (garbage-tailed) buffer.
TEST(ZstdCompressTest, DecompressRejectsTruncatedStream)
{
    const QByteArray compressed = ZstdCompress::compress(makeData(5000, 4));
    ASSERT_GT(compressed.size(), 10);

    // Drop half of the stream so the declared amount of data cannot be produced.
    const QByteArray truncated = compressed.left(compressed.size() / 2);
    EXPECT_TRUE(ZstdCompress::decompress(truncated).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Regression test: a tampered length prefix that claims fewer bytes than the stream actually
// decompresses to must not make decompress() loop forever, and must never yield more than the
// declared size.
TEST(ZstdCompressTest, DecompressDoesNotHangWhenStreamExpandsBeyondDeclaredSize)
{
    QByteArray compressed = ZstdCompress::compress(makeData(20000, 5));
    ASSERT_GT(compressed.size(), 8);

    const quint32 tampered = qToBigEndian<quint32>(16); // Claim only 16 bytes.
    memcpy(compressed.data(), &tampered, sizeof(quint32));

    const QByteArray result = ZstdCompress::decompress(compressed);
    EXPECT_LE(result.size(), 16); // Terminates; never overflows the declared buffer.
}
