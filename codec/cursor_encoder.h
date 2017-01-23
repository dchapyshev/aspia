//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/cursor_encoder.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__CURSOR_ENCODER_H
#define _ASPIA_CODEC__CURSOR_ENCODER_H

#include "aspia_config.h"

#include <stdint.h>

#include "base/macros.h"
#include "codec/compressor_zlib.h"
#include "desktop_capture/mouse_cursor_cache.h"

#include "proto/proto.pb.h"

namespace aspia {

class CursorEncoder
{
public:
    CursorEncoder();
    ~CursorEncoder();

    void Encode(proto::CursorShape *packet, std::unique_ptr<MouseCursor> mouse_cursor);

private:
    uint8_t* GetOutputBuffer(proto::CursorShape *packet, size_t size);
    void CompressCursor(proto::CursorShape *packet, const MouseCursor *mouse_cursor);

private:
    MouseCursorCache cache_;
    CompressorZLIB compressor_;

    DISALLOW_COPY_AND_ASSIGN(CursorEncoder);
};

} // namespace aspia

#endif // _ASPIA_CODEC__CURSOR_ENCODER_H
