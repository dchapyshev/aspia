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

#ifndef CODEC__CURSOR_ENCODER_H
#define CODEC__CURSOR_ENCODER_H

#include "base/macros_magic.h"
#include "codec/scoped_zstd_stream.h"
#include "desktop/mouse_cursor_cache.h"
#include "proto/desktop.pb.h"

namespace codec {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder() = default;

    bool encode(std::unique_ptr<desktop::MouseCursor> mouse_cursor,
                proto::CursorShape* cursor_shape);

private:
    bool compressCursor(proto::CursorShape* cursor_shape,
                        const desktop::MouseCursor* mouse_cursor);

    ScopedZstdCStream stream_;
    desktop::MouseCursorCache cache_;

    DISALLOW_COPY_AND_ASSIGN(CursorEncoder);
};

} // namespace codec

#endif // CODEC__CURSOR_ENCODER_H
