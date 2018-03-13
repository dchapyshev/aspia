//
// PROJECT:         Aspia
// FILE:            codec/video_decoder_zlib.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_zlib.h"

#include "codec/video_helpers.h"
#include "desktop_capture/desktop_frame_aligned.h"
#include "base/logging.h"

namespace aspia {

// static
std::unique_ptr<VideoDecoderZLIB> VideoDecoderZLIB::Create()
{
    return std::unique_ptr<VideoDecoderZLIB>(new VideoDecoderZLIB());
}

bool VideoDecoderZLIB::Decode(const proto::desktop::VideoPacket& packet,
                              DesktopFrame* target_frame)
{
    if (packet.has_format())
    {
        source_frame_ = DesktopFrameAligned::Create(
            ConvertFromVideoSize(packet.format().screen_size()),
            ConvertFromVideoPixelFormat(packet.format().pixel_format()));

        translator_ = PixelTranslator::Create(source_frame_->format(), target_frame->format());
    }

    DCHECK(source_frame_->size() == target_frame->size());

    if (!source_frame_ || !translator_)
    {
        LOG(LS_ERROR) << "A packet with image information was not received";
        return false;
    }

    const uint8_t* src = reinterpret_cast<const uint8_t*>(packet.data().data());
    const size_t src_size = packet.data().size();
    size_t used = 0;

    QRect frame_rect = QRect(QPoint(), source_frame_->size());

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = ConvertFromVideoRect(packet.dirty_rect(i));

        if (!frame_rect.contains(rect))
        {
            LOG(LS_ERROR) << "The rectangle is outside the screen area";
            return false;
        }

        uint8_t* dst = source_frame_->GetFrameDataAtPos(rect.x(), rect.y());
        const size_t row_size = rect.width() * source_frame_->format().BytesPerPixel();

        // Consume all the data in the message.
        bool decompress_again = true;

        int row_y = 0;
        size_t row_pos = 0;

        while (decompress_again && used < src_size)
        {
            // If we have reached the end of the current rectangle, then
            // proceed to the next one.
            if (row_y > rect.height() - 1)
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
                dst += source_frame_->stride();
            }
        }

        translator_->Translate(source_frame_->GetFrameDataAtPos(rect.topLeft()),
                               source_frame_->stride(),
                               target_frame->GetFrameDataAtPos(rect.topLeft()),
                               target_frame->stride(),
                               rect.width(),
                               rect.height());
    }

    decompressor_.Reset();

    return true;
}

} // namespace aspia
