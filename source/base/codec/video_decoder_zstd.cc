//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/video_decoder_zstd.h"

#include "base/logging.h"
#include "base/codec/pixel_translator.h"
#include "base/desktop/frame_aligned.h"

namespace base {

//--------------------------------------------------------------------------------------------------
VideoDecoderZstd::VideoDecoderZstd()
    : stream_(ZSTD_createDStream())
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
VideoDecoderZstd::~VideoDecoderZstd()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderZstd> VideoDecoderZstd::create()
{
    return std::unique_ptr<VideoDecoderZstd>(new VideoDecoderZstd());
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderZstd::decode(const proto::VideoPacket& packet, Frame* target_frame)
{
    if (packet.has_format())
    {
        const proto::VideoPacketFormat& format = packet.format();

        source_frame_ = FrameAligned::create(
            Size(format.video_rect().width(), format.video_rect().height()),
            base::PixelFormat::fromProto(format.pixel_format()), 32);

        translator_ = PixelTranslator::create(source_frame_->format(), PixelFormat::ARGB());
    }

    DCHECK(source_frame_->size() == target_frame->size());

    if (!source_frame_ || !translator_)
    {
        LOG(LS_ERROR) << "A packet with image information was not received";
        return false;
    }

    size_t ret = ZSTD_initDStream(stream_.get());
    if (ZSTD_isError(ret))
    {
        LOG(LS_ERROR) << "ZSTD_initDStream failed: " << ZSTD_getErrorName(ret);
        return false;
    }

    Rect frame_rect = Rect::makeSize(source_frame_->size());
    ZSTD_inBuffer input = { packet.data().data(), packet.data().size(), 0 };

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        Rect rect = base::Rect::fromProto(packet.dirty_rect(i));

        if (!frame_rect.containsRect(rect))
        {
            LOG(LS_ERROR) << "The rectangle is outside the screen area";
            return false;
        }

        quint8* output_data = source_frame_->frameDataAtPos(rect.x(), rect.y());
        const size_t output_size =
            static_cast<size_t>(rect.width() * source_frame_->format().bytesPerPixel());

        ZSTD_outBuffer output = { output_data, output_size, 0 };
        int row_y = 0;

        while (row_y < rect.height())
        {
            ret = ZSTD_decompressStream(stream_.get(), &output, &input);
            if (ZSTD_isError(ret))
            {
                LOG(LS_ERROR) << "ZSTD_decompressStream failed: " << ZSTD_getErrorName(ret);
                return false;
            }

            // If we completely unpacked the row in the rectangle.
            if (output.pos == output.size)
            {
                ++row_y;
                output_data += source_frame_->stride();
                output.dst = output_data;
                output.pos = 0;
            }
        }

        translator_->translate(source_frame_->frameDataAtPos(rect.topLeft()),
                               source_frame_->stride(),
                               target_frame->frameDataAtPos(rect.topLeft()),
                               target_frame->stride(),
                               rect.width(),
                               rect.height());
    }

    return true;
}

} // namespace base
