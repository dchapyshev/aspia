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

#ifndef BASE_CODEC_CURSOR_DECODER_H
#define BASE_CODEC_CURSOR_DECODER_H

#include <QByteArray>
#include <QVector>

#include "base/macros_magic.h"
#include "base/codec/scoped_zstd_stream.h"
#include "base/desktop/mouse_cursor.h"
#include "proto/desktop.h"

#include <optional>

namespace base {

class CursorDecoder
{
public:
    CursorDecoder();
    ~CursorDecoder();

    std::shared_ptr<MouseCursor> decode(const proto::CursorShape& cursor_shape);

    int cachedCursors() const;
    int takenCursorsFromCache() const;

private:
    QByteArray decompressCursor(const proto::CursorShape& cursor_shape) const;

    QVector<std::shared_ptr<MouseCursor>> cache_;
    std::optional<size_t> cache_size_;
    ScopedZstdDStream stream_;
    int taken_from_cache_ = 0;

    DISALLOW_COPY_AND_ASSIGN(CursorDecoder);
};

} // namespace base

#endif // BASE_CODEC_CURSOR_DECODER_H
