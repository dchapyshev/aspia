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

#ifndef CODEC__CURSOR_DECODER_H
#define CODEC__CURSOR_DECODER_H

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "codec/scoped_zstd_stream.h"

#include <optional>

namespace base {
class MouseCursor;
} // namespace base

namespace proto {
class CursorShape;
} // namespace proto

namespace codec {

class CursorDecoder
{
public:
    CursorDecoder();
    ~CursorDecoder();

    std::shared_ptr<base::MouseCursor> decode(const proto::CursorShape& cursor_shape);

private:
    base::ByteArray decompressCursor(const proto::CursorShape& cursor_shape);

    std::vector<std::shared_ptr<base::MouseCursor>> cache_;
    std::optional<size_t> cache_size_;
    ScopedZstdDStream stream_;

    DISALLOW_COPY_AND_ASSIGN(CursorDecoder);
};

} // namespace codec

#endif // CODEC__CURSOR_DECODER_H
