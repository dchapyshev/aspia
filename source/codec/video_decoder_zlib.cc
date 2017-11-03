//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_zlib.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_zlib.h"
#include "codec/video_helpers.h"
#include "base/logging.h"

namespace aspia {

// static
std::unique_ptr<VideoDecoderZLIB> VideoDecoderZLIB::Create()
{
    return std::unique_ptr<VideoDecoderZLIB>(new VideoDecoderZLIB());
}

bool VideoDecoderZLIB::Decode(const proto::VideoPacket& packet, DesktopFrame* frame)
{
    const uint8_t* src = reinterpret_cast<const uint8_t*>(packet.data().data());
    const size_t src_size = packet.data().size();
    size_t used = 0;

    DesktopRect frame_rect(DesktopRect::MakeSize(frame->Size()));

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        DesktopRect rect(ConvertFromVideoRect(packet.dirty_rect(i)));

        if (!frame_rect.ContainsRect(rect))
        {
            LOG(ERROR) << "The rectangle is outside the screen area";
            return false;
        }

        uint8_t* dst = frame->GetFrameDataAtPos(rect.x(), rect.y());
        const size_t row_size = rect.Width() * frame->Format().BytesPerPixel();

        // Consume all the data in the message.
        bool decompress_again = true;

        int row_y = 0;
        size_t row_pos = 0;

        while (decompress_again && used < src_size)
        {
            // If we have reached the end of the current rectangle, then
            // proceed to the next one.
            if (row_y > rect.Height() - 1)
                break;

            size_t written = 0;
            size_t consumed = 0;

            decompress_again = decompressor_.Process(src + used,
                                                     src_size - used,
                                                     dst + row_pos,
                                                     row_size - row_pos,
                                                     &consumed,
                                                     &written);
            used += consumed;
            row_pos += written;

            // If we completely unpacked the row in the rectangle
            if (row_pos == row_size)
            {
                ++row_y;
                row_pos = 0;
                dst += frame->Stride();
            }
        }
    }

    decompressor_.Reset();

    return true;
}

} // namespace aspia
