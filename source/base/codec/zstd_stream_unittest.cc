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

#include "base/codec/zstd_stream_compressor.h"
#include "base/codec/zstd_stream_decompressor.h"

#include <gtest/gtest.h>

#include <QByteArray>

namespace {

//--------------------------------------------------------------------------------------------------
// Terminal output: mostly repeating text, which is what the stream pair is used for.
QByteArray makeOutput(int size)
{
    QByteArray data;
    data.reserve(size);
    while (data.size() < size)
        data += "user@host:~$ ls -la /usr/share/doc\r\n";
    data.resize(size);
    return data;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// The stream survives whole and in order across many chunks. The decompressor bounds the window a
// stream may declare, and this is what proves the bound still admits what the compressor produces.
TEST(ZstdStreamTest, RoundTripOverManyChunks)
{
    ZstdStreamCompressor compressor;
    ZstdStreamDecompressor decompressor;

    QByteArray source;
    QByteArray restored;

    for (int i = 0; i < 64; ++i)
    {
        const QByteArray chunk = makeOutput(16 * 1024 + i);
        source += chunk;

        const std::string compressed = compressor.compress(chunk);
        ASSERT_FALSE(compressed.empty());

        restored += decompressor.decompress(compressed, 0);
    }

    EXPECT_EQ(restored, source);
}

//--------------------------------------------------------------------------------------------------
// A chunk larger than the output buffer of one decompression step comes back whole.
TEST(ZstdStreamTest, ChunkLargerThanTheOutputBuffer)
{
    ZstdStreamCompressor compressor;
    ZstdStreamDecompressor decompressor;

    const QByteArray source = makeOutput(4 * 1024 * 1024);

    const std::string compressed = compressor.compress(source);
    ASSERT_FALSE(compressed.empty());

    EXPECT_EQ(decompressor.decompress(compressed, 0), source);
}

//--------------------------------------------------------------------------------------------------
// The caller can cap how much one call is allowed to produce.
TEST(ZstdStreamTest, OutputSizeLimitIsRefused)
{
    ZstdStreamCompressor compressor;
    ZstdStreamDecompressor decompressor;

    const std::string compressed = compressor.compress(makeOutput(1024 * 1024));
    ASSERT_FALSE(compressed.empty());

    EXPECT_TRUE(decompressor.decompress(compressed, 1024).isEmpty());
}

//--------------------------------------------------------------------------------------------------
// Garbage from the peer is refused instead of being handed on as data.
TEST(ZstdStreamTest, CorruptStreamIsRefused)
{
    ZstdStreamDecompressor decompressor;

    const std::string garbage(256, '\x7f');
    EXPECT_TRUE(decompressor.decompress(garbage, 0).isEmpty());
}
