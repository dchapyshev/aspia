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

#ifndef CODEC__CURSOR_ENCODER_H
#define CODEC__CURSOR_ENCODER_H

#include "base/macros_magic.h"
#include "codec/scoped_zstd_stream.h"

#include <vector>

namespace desktop {
class MouseCursor;
} // namespace desktop

namespace proto {
class CursorShape;
} // namespace proto

namespace codec {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder();

    bool encode(const desktop::MouseCursor& mouse_cursor,
                proto::CursorShape* cursor_shape);

private:
    bool compressCursor(const desktop::MouseCursor& mouse_cursor,
                        proto::CursorShape* cursor_shape);

    ScopedZstdCStream stream_;
    std::vector<uint32_t> cache_;

    DISALLOW_COPY_AND_ASSIGN(CursorEncoder);
};

} // namespace codec

#endif // CODEC__CURSOR_ENCODER_H
