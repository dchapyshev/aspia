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

#ifndef _ASPIA_CODEC__CURSOR_DECODER_H
#define _ASPIA_CODEC__CURSOR_DECODER_H

#include "codec/decompressor_zlib.h"
#include "desktop_capture/mouse_cursor_cache.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class CursorDecoder
{
public:
    CursorDecoder() = default;
    ~CursorDecoder() = default;

    std::shared_ptr<MouseCursor> decode(const proto::desktop::CursorShape& cursor_shape);

private:
    bool decompressCursor(const proto::desktop::CursorShape& cursor_shape, quint8* image);

    std::unique_ptr<MouseCursorCache> cache_;
    DecompressorZLIB decompressor_;

    Q_DISABLE_COPY(CursorDecoder)
};

} // namespace aspia

#endif // _ASPIA_CODEC__CURSOR_DECODER_H
