//
// PROJECT:         Aspia
// FILE:            codec/cursor_encoder.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__CURSOR_ENCODER_H
#define _ASPIA_CODEC__CURSOR_ENCODER_H

#include "base/macros.h"
#include "codec/compressor_zlib.h"
#include "desktop_capture/mouse_cursor_cache.h"
#include "proto/desktop_session_message.pb.h"

namespace aspia {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder() = default;

    std::unique_ptr<proto::CursorShape> Encode(std::unique_ptr<MouseCursor> mouse_cursor);

private:
    void CompressCursor(proto::CursorShape* cursor_shape, const MouseCursor* mouse_cursor);

    CompressorZLIB compressor_;
    MouseCursorCache cache_;

    DISALLOW_COPY_AND_ASSIGN(CursorEncoder);
};

} // namespace aspia

#endif // _ASPIA_CODEC__CURSOR_ENCODER_H
