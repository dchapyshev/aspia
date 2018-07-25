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

#include "codec/cursor_encoder.h"

#include <QDebug>

namespace aspia {

namespace {

// Cache size can be in the range from 2 to 31.
constexpr uint8_t kCacheSize = 16;

// The compression ratio can be in the range of 1 to 9.
constexpr int kCompressionRatio = 6;

uint8_t* getOutputBuffer(proto::desktop::CursorShape* cursor_shape, size_t size)
{
    cursor_shape->mutable_data()->resize(size);
    return reinterpret_cast<uint8_t*>(cursor_shape->mutable_data()->data());
}

} // namespace

CursorEncoder::CursorEncoder()
    : compressor_(kCompressionRatio),
      cache_(kCacheSize)
{
    static_assert(kCacheSize >= 2 && kCacheSize <= 31);
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 9);
}

void CursorEncoder::compressCursor(proto::desktop::CursorShape* cursor_shape,
                                   const MouseCursor* mouse_cursor)
{
    compressor_.reset();

    int width = mouse_cursor->size().width();
    int height = mouse_cursor->size().height();

    const size_t row_size = width * sizeof(uint32_t);

    size_t packet_size = row_size * height;
    packet_size += packet_size / 100 + 16;

    uint8_t* compressed_pos = getOutputBuffer(cursor_shape, packet_size);
    const uint8_t* source_pos = mouse_cursor->data();

    size_t filled = 0;
    size_t row_pos = 0; // Position in the current row in bytes.
    int row_y = 0; // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // If we reached the last row in the rectangle.
        if (row_y == height - 1)
            flush = Compressor::CompressorFinish;

        size_t consumed = 0;
        size_t written = 0;

        compress_again = compressor_.process(
            source_pos + row_pos, row_size - row_pos,
            compressed_pos + filled, packet_size - filled,
            flush, &consumed, &written);

        row_pos += consumed;
        filled += written;

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            cursor_shape->mutable_data()->resize(filled);
            return;
        }

        // If we have reached the end of the current row in the rectangle and
        // this is not the last row.
        if (row_pos == row_size && row_y < height - 1)
        {
            row_pos = 0;
            source_pos += row_size;
            ++row_y;
        }
    }
}

std::unique_ptr<proto::desktop::CursorShape> CursorEncoder::encode(
    std::unique_ptr<MouseCursor> mouse_cursor)
{
    if (!mouse_cursor)
        return nullptr;

    const QSize& size = mouse_cursor->size();
    const int kMaxSize = std::numeric_limits<int16_t>::max() / 2;

    if (size.width() <= 0 || size.width() > kMaxSize ||
        size.height() <= 0 || size.height() > kMaxSize)
    {
        qWarning() << "Wrong size of cursor: " << size.width() << "x" << size.height();
        return nullptr;
    }

    std::unique_ptr<proto::desktop::CursorShape> cursor_shape =
        std::make_unique<proto::desktop::CursorShape>();

    size_t index = cache_.find(mouse_cursor.get());

    // The cursor is not found in the cache.
    if (index == MouseCursorCache::kInvalidIndex)
    {
        cursor_shape->set_width(size.width());
        cursor_shape->set_height(size.height());
        cursor_shape->set_hotspot_x(mouse_cursor->hotSpot().x());
        cursor_shape->set_hotspot_y(mouse_cursor->hotSpot().y());

        compressCursor(cursor_shape.get(), mouse_cursor.get());

        // If the cache is empty, then set the cache reset flag on the client
        // side and pass the maximum cache size.
        cursor_shape->set_flags(cache_.isEmpty() ?
            (proto::desktop::CursorShape::RESET_CACHE | (kCacheSize & 0x1F)) : 0);

        // Add the cursor to the cache.
        cache_.add(std::move(mouse_cursor));
    }
    else
    {
        cursor_shape->set_flags(proto::desktop::CursorShape::CACHE | (index & 0x1F));
    }

    return cursor_shape;
}

} // namespace aspia
