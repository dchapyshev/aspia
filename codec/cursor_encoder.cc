//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_encoder.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/cursor_encoder.h"

#include "base/logging.h"

namespace aspia {

// Cache size can be in the range from 2 to 31.
static const uint8_t kCacheSize = 16;

// The compression ratio can be in the range of 1 to 9.
static const int32_t kCompressionRatio = 6;

CursorEncoder::CursorEncoder() :
    compressor_(kCompressionRatio),
    cache_(kCacheSize)
{
    static_assert(kCacheSize >= 2 && kCacheSize <= 31, "Invalid cache size");
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 9, "Invalid compression ratio");
}

uint8_t* CursorEncoder::GetOutputBuffer(proto::CursorShape* cursor_shape, size_t size)
{
    cursor_shape->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(cursor_shape->mutable_data()->data()));
}

void CursorEncoder::CompressCursor(proto::CursorShape* cursor_shape, const MouseCursor* mouse_cursor)
{
    compressor_.Reset();

    int width = mouse_cursor->Size().Width();
    int height = mouse_cursor->Size().Height();

    const int row_size = width * sizeof(uint32_t);

    int packet_size = row_size * height;
    packet_size += packet_size / 100 + 16;

    uint8_t* compressed_pos = GetOutputBuffer(cursor_shape, packet_size);
    const uint8_t* source_pos = mouse_cursor->Data();

    int filled = 0;
    int row_pos = 0; // Position in the current row in bytes.
    int row_y = 0; // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // If we reached the last row in the rectangle.
        if (row_y == height - 1)
            flush = Compressor::CompressorFinish;

        int consumed = 0;
        int written = 0;

        compress_again = compressor_.Process(source_pos + row_pos, row_size - row_pos,
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

std::unique_ptr<proto::CursorShape> CursorEncoder::Encode(std::unique_ptr<MouseCursor> mouse_cursor)
{
    if (!mouse_cursor)
        return nullptr;

    const DesktopSize& size = mouse_cursor->Size();

    if (size.Width() <= 0 || size.Width() > (std::numeric_limits<int16_t>::max() / 2) ||
        size.Height() <= 0 || size.Height() > (std::numeric_limits<int16_t>::max() / 2))
    {
        DLOG(ERROR) << "Wrong size of cursor: " << size.Width() << "x" << size.Height();
        return nullptr;
    }

    std::unique_ptr<proto::CursorShape> cursor_shape =
        std::make_unique<proto::CursorShape>();

    size_t index = cache_.Find(mouse_cursor.get());

    // The cursor is not found in the cache.
    if (index == MouseCursorCache::kInvalidIndex)
    {
        cursor_shape->set_width(size.Width());
        cursor_shape->set_height(size.Height());
        cursor_shape->set_hotspot_x(mouse_cursor->Hotspot().x());
        cursor_shape->set_hotspot_y(mouse_cursor->Hotspot().y());

        CompressCursor(cursor_shape.get(), mouse_cursor.get());

        // If the cache is empty, then set the cache reset flag on the client
        // side and pass the maximum cache size.
        cursor_shape->set_flags(cache_.IsEmpty() ?
            (proto::CursorShape::RESET_CACHE | (kCacheSize & 0x1F)) : 0);

        // Add the cursor to the cache.
        cache_.Add(std::move(mouse_cursor));
    }
    else
    {
        cursor_shape->set_flags(proto::CursorShape::CACHE | (index & 0x1F));
    }

    return cursor_shape;
}

} // namespace aspia
