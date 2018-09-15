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

#include <fstream>
#include <vector>

#include "codec/compressor_zstd.h"
#include "codec/decompressor_zstd.h"

namespace aspia {

namespace {

using ImageBuffer = std::vector<uint8_t>;

bool readTestData(const std::string& file_path, ImageBuffer* buffer)
{
    std::ifstream file;
    file.open(file_path, std::ifstream::binary);
    if (!file.is_open())
        return false;

    file.seekg(0, file.end);
    size_t file_size = static_cast<size_t>(file.tellg());
    if (!file_size)
        return false;

    buffer->resize(file_size);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer->data()), buffer->size());
    if (file.fail())
        return false;

    return true;
}

ImageBuffer compress(int compress_ratio, const ImageBuffer& input)
{
    CompressorZstd compressor(compress_ratio);

    compressor.reset();

    ImageBuffer compressed_buffer;
    compressed_buffer.resize(compressor.compressBound(input.size()));

    size_t filled = 0;  // Number of bytes in the destination buffer.
    size_t pos = 0;  // Position in the current row in bytes.
    bool compress_again = true;

    while (compress_again)
    {
        // Number of bytes that was taken from the source buffer.
        size_t consumed = 0;

        // Number of bytes that were written to the destination buffer.
        size_t written = 0;

        compress_again = compressor.process(input.data() + pos,
                                            input.size() - pos,
                                            compressed_buffer.data() + filled,
                                            compressed_buffer.size() - filled,
                                            Compressor::CompressorFinish,
                                            &consumed,
                                            &written);

        pos += consumed;
        filled += written;

        // If we have filled the message or we have reached the end of stream.
        if (filled == compressed_buffer.size() || !compress_again)
        {
            compressed_buffer.resize(filled);
            return compressed_buffer;
        }
    }

    return ImageBuffer();
}

ImageBuffer decompress(size_t output_size, const ImageBuffer& input)
{
    DecompressorZstd decompressor;

    ImageBuffer decompressed_buffer;
    decompressed_buffer.resize(output_size);

    // Consume all the data in the message.
    bool decompress_again = true;

    size_t used = 0;
    size_t pos = 0;

    while (decompress_again)
    {
        size_t written = 0;
        size_t consumed = 0;

        decompress_again = decompressor.process(input.data() + used,
                                                input.size() - used,
                                                decompressed_buffer.data() + pos,
                                                decompressed_buffer.size() - pos,
                                                &consumed,
                                                &written);
        used += consumed;
        pos += written;
    }

    return decompressed_buffer;
}

} // namespace

TEST(compressor_zstd_test, compress_decompress)
{
    ImageBuffer source_buffer;

    bool ret = readTestData("test_data/desktop_image_1.bmp", &source_buffer);
    ASSERT_TRUE(ret);
    ASSERT_TRUE(!source_buffer.empty());

    ImageBuffer compressed_buffer = compress(8, source_buffer);
    ASSERT_TRUE(!compressed_buffer.empty());

    ImageBuffer decompressed_buffer = decompress(source_buffer.size(), compressed_buffer);
    ASSERT_TRUE(!decompressed_buffer.empty());

    ASSERT_EQ(source_buffer.size(), decompressed_buffer.size());

    bool equal = memcmp(source_buffer.data(), decompressed_buffer.data(), source_buffer.size()) == 0;
    ASSERT_TRUE(equal);

    std::cerr << "Source size: " << source_buffer.size() << std::endl;
    std::cerr << "Compressed size: " << compressed_buffer.size() << std::endl;
}

TEST(compressor_zstd_test, DISABLED_benchmark)
{
    ImageBuffer source_buffer1;
    ImageBuffer source_buffer2;

    bool ret = readTestData("test_data/desktop_image_1.bmp", &source_buffer1);
    ASSERT_TRUE(ret);

    ret = readTestData("test_data/desktop_image_2.bmp", &source_buffer2);
    ASSERT_TRUE(ret);

    for (int i = 0; i < 100; ++i)
    {
        // Image 1.
        ImageBuffer compressed_buffer1 = compress(8, source_buffer1);
        ASSERT_TRUE(!compressed_buffer1.empty());

        ImageBuffer decompressed_buffer1 = decompress(source_buffer1.size(), compressed_buffer1);
        ASSERT_TRUE(!decompressed_buffer1.empty());

        ASSERT_EQ(source_buffer1.size(), decompressed_buffer1.size());

        bool equal = memcmp(source_buffer1.data(), decompressed_buffer1.data(), source_buffer1.size()) == 0;
        ASSERT_TRUE(equal);

        // Image 2.
        ImageBuffer compressed_buffer2 = compress(8, source_buffer2);
        ASSERT_TRUE(!compressed_buffer2.empty());

        ImageBuffer decompressed_buffer2 = decompress(source_buffer2.size(), compressed_buffer2);
        ASSERT_TRUE(!decompressed_buffer2.empty());

        ASSERT_EQ(source_buffer2.size(), decompressed_buffer2.size());

        equal = memcmp(source_buffer2.data(), decompressed_buffer2.data(), source_buffer2.size()) == 0;
        ASSERT_TRUE(equal);
    }
}

} // namespace aspia
