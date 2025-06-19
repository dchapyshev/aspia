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

#include "base/codec/cursor_decoder.h"

#include "base/logging.h"

namespace base {

namespace {

constexpr qsizetype kMinCacheSize = 2;
constexpr qsizetype kMaxCacheSize = 30;

} // namespace

//--------------------------------------------------------------------------------------------------
CursorDecoder::CursorDecoder()
    : stream_(ZSTD_createDStream())
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
CursorDecoder::~CursorDecoder()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QByteArray CursorDecoder::decompressCursor(const proto::desktop::CursorShape& cursor_shape) const
{
    const std::string& data = cursor_shape.data();

    if (data.empty())
    {
        LOG(ERROR) << "No cursor data";
        return QByteArray();
    }

    if (cursor_shape.width() <= 0 || cursor_shape.height() <= 0)
    {
        LOG(ERROR) << "Invalid cursor size:" << cursor_shape;
        return QByteArray();
    }

    QByteArray image;
    image.resize(static_cast<size_t>(cursor_shape.width()) *
                 static_cast<size_t>(cursor_shape.height()) *
                 sizeof(quint32));

    size_t ret = ZSTD_initDStream(stream_.get());
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_initDStream failed:" << ZSTD_getErrorName(ret) << "(" << ret << ")";
        return QByteArray();
    }

    ZSTD_inBuffer input = { data.data(), data.size(), 0 };
    ZSTD_outBuffer output = { image.data(), static_cast<size_t>(image.size()), 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_decompressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(ERROR) << "ZSTD_decompressStream failed:" << ZSTD_getErrorName(ret)
                       << "(" << ret << ")";
            return QByteArray();
        }
    }

    return image;
}

//--------------------------------------------------------------------------------------------------
std::shared_ptr<MouseCursor> CursorDecoder::decode(const proto::desktop::CursorShape& cursor_shape)
{
    int cache_index;

    if (cursor_shape.flags() & proto::desktop::CursorShape::CACHE)
    {
        if (!cache_size_.has_value())
        {
            LOG(ERROR) << "Host did not send cache reset command";
            return nullptr;
        }

        // Bits 0-4 contain the cursor position in the cache.
        cache_index = cursor_shape.flags() & 0x1F;
        ++taken_from_cache_;
    }
    else
    {
        QSize size(cursor_shape.width(), cursor_shape.height());
        QPoint hotspot(cursor_shape.hotspot_x(), cursor_shape.hotspot_y());
        QPoint dpi(cursor_shape.dpi_x(), cursor_shape.dpi_y());

        if (size.width()  <= 0 || size.width()  > (std::numeric_limits<qint16>::max() / 2) ||
            size.height() <= 0 || size.height() > (std::numeric_limits<qint16>::max() / 2))
        {
            LOG(ERROR) << "Cursor dimensions are out of bounds for SetCursor:" << size;
            return nullptr;
        }

        QByteArray image = decompressCursor(cursor_shape);
        if (image.isEmpty())
        {
            LOG(ERROR) << "decompressCursor failed";
            return nullptr;
        }

        if (dpi.x() <= 0 || dpi.x() > std::numeric_limits<qint16>::max() ||
            dpi.y() <= 0 || dpi.y() > std::numeric_limits<qint16>::max())
        {
            dpi = QPoint(MouseCursor::kDefaultDpiX, MouseCursor::kDefaultDpiY);
        }

        std::unique_ptr<MouseCursor> mouse_cursor =
            std::make_unique<MouseCursor>(std::move(image), size, hotspot, dpi);

        if (cursor_shape.flags() & proto::desktop::CursorShape::RESET_CACHE)
        {
            qsizetype cache_size = cursor_shape.flags() & 0x1F;

            if (cache_size < kMinCacheSize || cache_size > kMaxCacheSize)
            {
                LOG(ERROR) << "Invalid cache size:" << cache_size;
                return nullptr;
            }

            cache_size_.emplace(cache_size);
            cache_.reserve(cache_size);
            cache_.clear();
        }

        if (!cache_size_.has_value())
        {
            LOG(ERROR) << "Host did not send cache reset command";
            return nullptr;
        }

        // Add the cursor to the end of the list.
        cache_.push_back(std::move(mouse_cursor));

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
        LOG(ERROR) << "Invalid cache index:" << cache_index;
        return nullptr;
    }

    return cache_[cache_index];
}

//--------------------------------------------------------------------------------------------------
int CursorDecoder::cachedCursors() const
{
    return static_cast<int>(cache_.size());
}

//--------------------------------------------------------------------------------------------------
int CursorDecoder::takenCursorsFromCache() const
{
    return taken_from_cache_;
}

} // namespace base
