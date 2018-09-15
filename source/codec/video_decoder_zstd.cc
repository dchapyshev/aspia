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

#include "codec/video_decoder_zstd.h"

#include "base/logging.h"
#include "codec/decompressor_zstd.h"
#include "codec/pixel_translator.h"
#include "codec/video_util.h"
#include "desktop_capture/desktop_frame_aligned.h"

namespace aspia {

VideoDecoderZstd::VideoDecoderZstd()
    : decompressor_(std::make_unique<DecompressorZstd>())
{
    // Nothing
}

// static
std::unique_ptr<VideoDecoderZstd> VideoDecoderZstd::create()
{
    return std::unique_ptr<VideoDecoderZstd>(new VideoDecoderZstd());
}

bool VideoDecoderZstd::decode(const proto::desktop::VideoPacket& packet,
                              DesktopFrame* target_frame)
{
    if (packet.has_format())
    {
        const proto::desktop::VideoPacketFormat& format = packet.format();

        source_frame_ = DesktopFrameAligned::create(
            DesktopSize(format.screen_rect().width(), format.screen_rect().height()),
            VideoUtil::fromVideoPixelFormat(format.pixel_format()), 32);

        translator_ = PixelTranslator::create(source_frame_->format(), target_frame->format());
    }

    DCHECK(source_frame_->size() == target_frame->size());

    if (!source_frame_ || !translator_)
    {
        LOG(LS_WARNING) << "A packet with image information was not received";
        return false;
    }

    const uint8_t* src = reinterpret_cast<const uint8_t*>(packet.data().data());
    const size_t src_size = packet.data().size();
    size_t used = 0;

    DesktopRect frame_rect = DesktopRect::makeSize(source_frame_->size());

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        DesktopRect rect = VideoUtil::fromVideoRect(packet.dirty_rect(i));

        if (!frame_rect.containsRect(rect))
        {
            LOG(LS_WARNING) << "The rectangle is outside the screen area";
            return false;
        }

        uint8_t* dst = source_frame_->frameDataAtPos(rect.x(), rect.y());
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

            decompress_again = decompressor_->process(src + used,
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

    decompressor_->reset();
    return true;
}

} // namespace aspia
