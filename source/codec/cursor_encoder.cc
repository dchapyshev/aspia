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

namespace codec {

namespace {

// Cache size can be in the range from 2 to 31.
constexpr uint8_t kCacheSize = 16;

// The compression ratio can be in the range of 1 to 22.
constexpr int kCompressionRatio = 8;

uint8_t* outputBuffer(proto::CursorShape* cursor_shape, size_t size)
{
    cursor_shape->mutable_data()->resize(size);
    return reinterpret_cast<uint8_t*>(cursor_shape->mutable_data()->data());
}

} // namespace

CursorEncoder::CursorEncoder()
    : stream_(ZSTD_createCStream()),
      cache_(kCacheSize)
{
    static_assert(kCacheSize >= 2 && kCacheSize <= 31);
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 22);
}

bool CursorEncoder::compressCursor(proto::CursorShape* cursor_shape,
                                   const desktop::MouseCursor* mouse_cursor)
{
    size_t ret = ZSTD_initCStream(stream_.get(), kCompressionRatio);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

    const size_t input_size = mouse_cursor->stride() * mouse_cursor->size().height();
    const uint8_t* input_data = mouse_cursor->data();

    const size_t output_size = ZSTD_compressBound(input_size);
    uint8_t* output_data = outputBuffer(cursor_shape, output_size);

    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_compressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(LS_WARNING) << "ZSTD_compressStream failed: " << ZSTD_getErrorName(ret);
            return false;
        }
    }

    ret = ZSTD_endStream(stream_.get(), &output);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

    cursor_shape->mutable_data()->resize(output.pos);
    return true;
}

bool CursorEncoder::encode(std::unique_ptr<desktop::MouseCursor> mouse_cursor,
                           proto::CursorShape* cursor_shape)
{
    if (!mouse_cursor)
        return false;

    const desktop::Size& size = mouse_cursor->size();
    const int kMaxSize = std::numeric_limits<int16_t>::max() / 2;

    if (size.width() <= 0 || size.width() > kMaxSize ||
        size.height() <= 0 || size.height() > kMaxSize)
    {
        LOG(LS_WARNING) << "Wrong size of cursor: " << size.width() << "x" << size.height();
        return false;
    }

    size_t index = cache_.find(mouse_cursor.get());

    // The cursor is not found in the cache.
    if (index == desktop::MouseCursorCache::kInvalidIndex)
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
            (proto::CursorShape::RESET_CACHE | (kCacheSize & 0x1F)) : 0);

        // Add the cursor to the cache.
        cache_.add(std::move(mouse_cursor));
    }
    else
    {
        cursor_shape->set_flags(proto::CursorShape::CACHE | (index & 0x1F));
    }

    return true;
}

} // namespace codec
