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

#include "base/logging.h"

namespace aspia {

namespace {

// Cache size can be in the range from 2 to 31.
constexpr uint8_t kCacheSize = 16;

// The compression ratio can be in the range of 1 to 22.
constexpr int kCompressionRatio = 8;

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
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 22);
}

bool CursorEncoder::compressCursor(proto::desktop::CursorShape* cursor_shape,
                                   const MouseCursor* mouse_cursor)
{
    compressor_.reset();

    const size_t input_size = mouse_cursor->stride() * mouse_cursor->size().height();
    const uint8_t* input = mouse_cursor->data();

    const size_t output_size = compressor_.compressBound(input_size);
    uint8_t* output = getOutputBuffer(cursor_shape, output_size);

    size_t filled = 0;
    size_t pos = 0;
    bool compress_again = true;

    while (compress_again)
    {
        size_t consumed = 0;
        size_t written = 0;

        compress_again = compressor_.process(input + pos,
                                             input_size - pos,
                                             output + filled,
                                             output_size - filled,
                                             Compressor::CompressorFinish,
                                             &consumed,
                                             &written);

        pos += consumed;
        filled += written;

        // If we have filled the message or we have reached the end of stream.
        if (filled == output_size || !compress_again)
        {
            cursor_shape->mutable_data()->resize(filled);
            return true;
        }
    }

    return false;
}

bool CursorEncoder::encode(std::unique_ptr<MouseCursor> mouse_cursor,
                           proto::desktop::CursorShape* cursor_shape)
{
    if (!mouse_cursor)
        return false;

    const DesktopSize& size = mouse_cursor->size();
    const int kMaxSize = std::numeric_limits<int16_t>::max() / 2;

    if (size.width() <= 0 || size.width() > kMaxSize ||
        size.height() <= 0 || size.height() > kMaxSize)
    {
        LOG(LS_WARNING) << "Wrong size of cursor: " << size.width() << "x" << size.height();
        return false;
    }

    size_t index = cache_.find(mouse_cursor.get());

    // The cursor is not found in the cache.
    if (index == MouseCursorCache::kInvalidIndex)
    {
        cursor_shape->set_width(size.width());
        cursor_shape->set_height(size.height());
        cursor_shape->set_hotspot_x(mouse_cursor->hotSpot().x());
        cursor_shape->set_hotspot_y(mouse_cursor->hotSpot().y());

        if (!compressCursor(cursor_shape, mouse_cursor.get()))
            return false;

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

    return true;
}

} // namespace aspia
