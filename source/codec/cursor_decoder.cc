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

#include "codec/cursor_decoder.h"

#include "base/logging.h"
#include "desktop/mouse_cursor_cache.h"

namespace codec {

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
            LOG(LS_WARNING) << "ZSTD_decompressStream failed: " << ZSTD_getErrorName(ret);
            return base::ByteArray();
        }
    }

    return image;
}

std::shared_ptr<desktop::MouseCursor> CursorDecoder::decode(const proto::CursorShape& cursor_shape)
{
    size_t cache_index;

    if (cursor_shape.flags() & proto::CursorShape::CACHE)
    {
        // Bits 0-4 contain the cursor position in the cache.
        cache_index = cursor_shape.flags() & 0x1F;
    }
    else
    {
        desktop::Size size(cursor_shape.width(), cursor_shape.height());

        if (size.width()  <= 0 || size.width()  > (std::numeric_limits<int16_t>::max() / 2) ||
            size.height() <= 0 || size.height() > (std::numeric_limits<int16_t>::max() / 2))
        {
            LOG(LS_WARNING) << "Cursor dimensions are out of bounds for SetCursor: "
                            << size.width() << "x" << size.height();
            return nullptr;
        }

        base::ByteArray image = decompressCursor(cursor_shape);
        if (image.empty())
            return nullptr;

        std::unique_ptr<desktop::MouseCursor> mouse_cursor =
            std::make_unique<desktop::MouseCursor>(
                std::move(image),
                size,
                desktop::Point(cursor_shape.hotspot_x(), cursor_shape.hotspot_y()));

        if (cursor_shape.flags() & proto::CursorShape::RESET_CACHE)
        {
            size_t cache_size = cursor_shape.flags() & 0x1F;

            if (!desktop::MouseCursorCache::isValidCacheSize(cache_size))
                return nullptr;

            cache_ = std::make_unique<desktop::MouseCursorCache>(cache_size);
        }

        if (!cache_)
        {
            LOG(LS_WARNING) << "Host did not send cache reset command";
            return nullptr;
        }

        cache_index = cache_->add(std::move(mouse_cursor));
    }

    return cache_->get(cache_index);
}

} // namespace codec
