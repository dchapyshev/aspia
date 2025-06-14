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

#ifndef BASE_CODEC_CURSOR_ENCODER_H
#define BASE_CODEC_CURSOR_ENCODER_H

#include <QVector>

#include "base/codec/scoped_zstd_stream.h"
#include "base/desktop/mouse_cursor.h"
#include "proto/desktop.h"

namespace base {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder();

    bool encode(const MouseCursor& mouse_cursor, proto::desktop::CursorShape* cursor_shape);

private:
    bool compressCursor(const MouseCursor& mouse_cursor, proto::desktop::CursorShape* cursor_shape) const;

    ScopedZstdCStream stream_;
    QVector<quint32> cache_;

    Q_DISABLE_COPY(CursorEncoder)
};

} // namespace base

#endif // BASE_CODEC_CURSOR_ENCODER_H
