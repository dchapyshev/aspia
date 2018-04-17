//
// PROJECT:         Aspia
// FILE:            codec/cursor_encoder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__CURSOR_ENCODER_H
#define _ASPIA_CODEC__CURSOR_ENCODER_H

#include <memory>

#include "codec/compressor_zlib.h"
#include "desktop_capture/mouse_cursor_cache.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder() = default;

    std::unique_ptr<proto::desktop::CursorShape> encode(std::unique_ptr<MouseCursor> mouse_cursor);

private:
    void CompressCursor(proto::desktop::CursorShape* cursor_shape,
                        const MouseCursor* mouse_cursor);

    CompressorZLIB compressor_;
    MouseCursorCache cache_;

    Q_DISABLE_COPY(CursorEncoder)
};

} // namespace aspia

#endif // _ASPIA_CODEC__CURSOR_ENCODER_H
