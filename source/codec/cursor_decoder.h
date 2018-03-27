//
// PROJECT:         Aspia
// FILE:            codec/cursor_decoder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__CURSOR_DECODER_H
#define _ASPIA_CODEC__CURSOR_DECODER_H

#include "codec/decompressor_zlib.h"
#include "desktop_capture/mouse_cursor_cache.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class CursorDecoder
{
public:
    CursorDecoder() = default;
    ~CursorDecoder() = default;

    std::shared_ptr<MouseCursor> Decode(const proto::desktop::CursorShape& cursor_shape);

private:
    bool DecompressCursor(const proto::desktop::CursorShape& cursor_shape, quint8* image);

    std::unique_ptr<MouseCursorCache> cache_;
    DecompressorZLIB decompressor_;

    Q_DISABLE_COPY(CursorDecoder)
};

} // namespace aspia

#endif // _ASPIA_CODEC__CURSOR_DECODER_H
