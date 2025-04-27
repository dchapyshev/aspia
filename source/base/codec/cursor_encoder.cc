//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/cursor_encoder.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "proto/desktop.h"

#include <libyuv/compare.h>

namespace base {

namespace {

// Cache size can be in the range from 2 to 30.
constexpr size_t kCacheSize = 30;

// The compression ratio can be in the range of 1 to 22.
constexpr int kCompressionRatio = 12;

// Recommended seed value for a hash.
constexpr uint32_t kHashingSeed = 5381;

//--------------------------------------------------------------------------------------------------
uint8_t* outputBuffer(proto::CursorShape* cursor_shape, size_t size)
{
    cursor_shape->mutable_data()->resize(size);
    return reinterpret_cast<uint8_t*>(cursor_shape->mutable_data()->data());
}

} // namespace

//--------------------------------------------------------------------------------------------------
CursorEncoder::CursorEncoder()
    : stream_(ZSTD_createCStream())
{
    LOG(LS_INFO) << "Ctor";

    static_assert(kCacheSize >= 2 && kCacheSize <= 30);
    static_assert(kCompressionRatio >= 1 && kCompressionRatio <= 22);

    // Reserve memory for the maximum number of elements in the cache.
    cache_.reserve(kCacheSize);
}

//--------------------------------------------------------------------------------------------------
CursorEncoder::~CursorEncoder()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool CursorEncoder::compressCursor(
    const MouseCursor& mouse_cursor, proto::CursorShape* cursor_shape) const
{
    if (!cursor_shape)
    {
        LOG(LS_ERROR) << "Invalid pointer";
        return false;
    }

    if (mouse_cursor.width() <= 0 || mouse_cursor.height() <= 0)
    {
        LOG(LS_ERROR) << "Invalid cursor size: "
                      << mouse_cursor.width() << "x" << mouse_cursor.height();
        return false;
    }

    const QByteArray& image = mouse_cursor.constImage();
    if (image.isEmpty())
    {
        LOG(LS_ERROR) << "Invalid cursor image buffer";
        return false;
    }

    size_t ret = ZSTD_initCStream(stream_.get(), kCompressionRatio);
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_initCStream failed: " << ZSTD_getErrorName(ret)
                      << " (" << ret << ")";
        return false;
    }

    const size_t input_size = image.size();
    const uint8_t* input_data = reinterpret_cast<const uint8_t*>(image.data());

    const size_t output_size = ZSTD_compressBound(input_size);
    uint8_t* output_data = outputBuffer(cursor_shape, output_size);

    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_compressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(LS_ERROR) << "ZSTD_compressStream failed: " << ZSTD_getErrorName(ret)
                          << " (" << ret << ")";
            return false;
        }
    }

    ret = ZSTD_endStream(stream_.get(), &output);
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_endStream failed: " << ZSTD_getErrorName(ret)
                      << " (" << ret << ")";
        return false;
    }

    cursor_shape->mutable_data()->resize(output.pos);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool CursorEncoder::encode(const MouseCursor& mouse_cursor, proto::CursorShape* cursor_shape)
{
    const Size& size = mouse_cursor.size();
    const int kMaxSize = std::numeric_limits<int16_t>::max() / 2;

    // Check the correctness of the cursor size.
    if (size.width() <= 0 || size.width() > kMaxSize ||
        size.height() <= 0 || size.height() > kMaxSize)
    {
        LOG(LS_ERROR) << "Wrong size of cursor: " << size.width() << "x" << size.height();
        return false;
    }

    // Calculate the hash of the cursor to search in the cache.
    uint32_t hash = libyuv::HashDjb2(reinterpret_cast<const uint8_t*>(mouse_cursor.constImage().data()),
                                     mouse_cursor.constImage().size(),
                                     kHashingSeed);

    // Trying to find cursor in cache.
    for (size_t index = 0; index < cache_.size(); ++index)
    {
        if (cache_[index] == hash)
        {
            // Cursor found in cache.
            cursor_shape->set_flags(proto::CursorShape::CACHE | (index & 0x1F));
            return true;
        }
    }

    Point dpi = mouse_cursor.constDpi();
    if (dpi.x() <= 0 || dpi.y() <= 0)
        dpi = Point(MouseCursor::kDefaultDpiX, MouseCursor::kDefaultDpiY);

    // Set cursor parameters.
    cursor_shape->set_width(size.width());
    cursor_shape->set_height(size.height());
    cursor_shape->set_hotspot_x(mouse_cursor.hotSpot().x());
    cursor_shape->set_hotspot_y(mouse_cursor.hotSpot().y());
    cursor_shape->set_dpi_x(dpi.x());
    cursor_shape->set_dpi_y(dpi.y());

    // Compress the cursor using ZSTD.
    if (!compressCursor(mouse_cursor, cursor_shape))
    {
        LOG(LS_ERROR) << "compressCursor failed";
        return false;
    }

    if (cache_.empty())
    {
        LOG(LS_INFO) << "Empty cursor cache";

        // If the cache is empty, then set the cache reset flag on the client side and pass the
        // maximum cache size.
        cursor_shape->set_flags(proto::CursorShape::RESET_CACHE | (kCacheSize & 0x1F));
    }

    // Add the cursor to the cache.
    cache_.emplace_back(hash);

    // If the current cache size exceeds the maximum cache size.
    if (cache_.size() > kCacheSize)
    {
        // Delete the first element in the cache (the oldest one).
        cache_.erase(cache_.begin());
    }

    return true;
}

} // namespace base
