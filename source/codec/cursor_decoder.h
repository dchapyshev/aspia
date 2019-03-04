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

#ifndef CODEC__CURSOR_DECODER_H
#define CODEC__CURSOR_DECODER_H

#include "base/macros_magic.h"
#include "codec/scoped_zstd_stream.h"
#include "proto/desktop.pb.h"

#include <memory>

namespace desktop {
class MouseCursor;
class MouseCursorCache;
} // namespace aspia

namespace codec {

class CursorDecoder
{
public:
    CursorDecoder();
    ~CursorDecoder();

    std::shared_ptr<desktop::MouseCursor> decode(const proto::desktop::CursorShape& cursor_shape);

private:
    bool decompressCursor(const proto::desktop::CursorShape& cursor_shape,
                          uint8_t* output_data,
                          size_t output_size);

    std::unique_ptr<desktop::MouseCursorCache> cache_;
    ScopedZstdDStream stream_;

    DISALLOW_COPY_AND_ASSIGN(CursorDecoder);
};

} // namespace codec

#endif // CODEC__CURSOR_DECODER_H
