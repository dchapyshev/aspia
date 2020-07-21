//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "proto/desktop.pb.h"

namespace base {

namespace {

constexpr size_t kMinCacheSize = 2;
constexpr size_t kMaxCacheSize = 31;

} // namespace

CursorDecoder::CursorDecoder()
    : stream_(ZSTD_createDStream())
{
    // Nothing
}

CursorDecoder::~CursorDecoder() = default;

base::ByteArray CursorDecoder::decompressCursor(const proto::CursorShape& cursor_shape)
{
    const std::string& data = cursor_shape.data();

    if (data.empty())
        return base::ByteArray();

    base::ByteArray image;
    image.resize(cursor_shape.width() * cursor_shape.height() * sizeof(uint32_t));

    size_t ret = ZSTD_initDStream(stream_.get());
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

    ZSTD_inBuffer input = { data.data(), data.size(), 0 };
    ZSTD_outBuffer output = { image.data(), image.size(), 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_decompressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(LS_ERROR) << "ZSTD_decompressStream failed: " << ZSTD_getErrorName(ret);
            return base::ByteArray();
        }
    }

    return image;
}

std::shared_ptr<base::MouseCursor> CursorDecoder::decode(const proto::CursorShape& cursor_shape)
{
    size_t cache_index;

    if (cursor_shape.flags() & proto::CursorShape::CACHE)
    {
        if (!cache_size_.has_value())
        {
            LOG(LS_ERROR) << "Host did not send cache reset command";
            return nullptr;
        }

        // Bits 0-4 contain the cursor position in the cache.
        cache_index = cursor_shape.flags() & 0x1F;
    }
    else
    {
        base::Size size(cursor_shape.width(), cursor_shape.height());
        base::Point hotspot(cursor_shape.hotspot_x(), cursor_shape.hotspot_y());

        if (size.width()  <= 0 || size.width()  > (std::numeric_limits<int16_t>::max() / 2) ||
            size.height() <= 0 || size.height() > (std::numeric_limits<int16_t>::max() / 2))
        {
            LOG(LS_ERROR) << "Cursor dimensions are out of bounds for SetCursor: "
                          << size.width() << "x" << size.height();
            return nullptr;
        }

        base::ByteArray image = decompressCursor(cursor_shape);
        if (image.empty())
            return nullptr;

        std::unique_ptr<base::MouseCursor> mouse_cursor =
            std::make_unique<base::MouseCursor>(std::move(image), size, hotspot);

        if (cursor_shape.flags() & proto::CursorShape::RESET_CACHE)
        {
            size_t cache_size = cursor_shape.flags() & 0x1F;

            if (cache_size < kMinCacheSize || cache_size > kMaxCacheSize)
                return nullptr;

            cache_size_.emplace(cache_size);
            cache_.reserve(cache_size);
            cache_.clear();
        }

        if (!cache_size_.has_value())
        {
            LOG(LS_ERROR) << "Host did not send cache reset command";
            return nullptr;
        }

        // Add the cursor to the end of the list.
        cache_.emplace_back(std::move(mouse_cursor));

        // If the current cache size exceeds the maximum cache size.
        if (cache_.size() > cache_size_.value())
        {
            // Delete the first element in the cache (the oldest one).
            cache_.erase(cache_.begin());
        }

        cache_index = cache_.size() - 1;
    }

    if (cache_index >= cache_.size())
    {
        LOG(LS_ERROR) << "Invalid cache index: " << cache_index;
        return nullptr;
    }

    return cache_.at(cache_index);
}

} // namespace base
