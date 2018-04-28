//
// PROJECT:         Aspia
// FILE:            codec/video_decoder_zlib.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_zlib.h"

#include <QDebug>

#include "codec/video_util.h"
#include "desktop_capture/desktop_frame_aligned.h"

namespace aspia {

// static
std::unique_ptr<VideoDecoderZLIB> VideoDecoderZLIB::create()
{
    return std::unique_ptr<VideoDecoderZLIB>(new VideoDecoderZLIB());
}

bool VideoDecoderZLIB::decode(const proto::desktop::VideoPacket& packet,
                              DesktopFrame* target_frame)
{
    if (packet.has_format())
    {
        source_frame_ = DesktopFrameAligned::create(
            VideoUtil::fromVideoSize(packet.format().screen_size()),
            VideoUtil::fromVideoPixelFormat(packet.format().pixel_format()));

        translator_ = PixelTranslator::create(source_frame_->format(), target_frame->format());
    }

    Q_ASSERT(source_frame_->size() == target_frame->size());

    if (!source_frame_ || !translator_)
    {
        qWarning("A packet with image information was not received");
        return false;
    }

    const quint8* src = reinterpret_cast<const quint8*>(packet.data().data());
    const size_t src_size = packet.data().size();
    size_t used = 0;

    QRect frame_rect = QRect(QPoint(), source_frame_->size());

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = VideoUtil::fromVideoRect(packet.dirty_rect(i));

        if (!frame_rect.contains(rect))
        {
            qWarning("The rectangle is outside the screen area");
            return false;
        }

        quint8* dst = source_frame_->frameDataAtPos(rect.x(), rect.y());
        const size_t row_size = rect.width() * source_frame_->format().bytesPerPixel();

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

            decompress_again = decompressor_.process(src + used,
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

        translator_->translate(source_frame_->frameDataAtPos(rect.topLeft()),
                               source_frame_->stride(),
                               target_frame->frameDataAtPos(rect.topLeft()),
                               target_frame->stride(),
                               rect.width(),
                               rect.height());
    }

    decompressor_.reset();
    return true;
}

} // namespace aspia
