//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/video_decoder_h264_sw.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/frame.h"
#include "proto/desktop_video.h"

#include <libyuv/convert_argb.h>

#include <wels/codec_api.h>
#include <wels/codec_def.h>

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderH264SW> VideoDecoderH264SW::create()
{
    std::unique_ptr<VideoDecoderH264SW> instance(new VideoDecoderH264SW());
    if (!instance->initialize())
        return nullptr;
    return instance;
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264SW::~VideoDecoderH264SW() = default;

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264SW::DecoderDeleter::operator()(ISVCDecoder* decoder) const
{
    if (decoder)
    {
        decoder->Uninitialize();
        WelsDestroyDecoder(decoder);
    }
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264SW::initialize()
{
    ISVCDecoder* raw_decoder = nullptr;
    if (WelsCreateDecoder(&raw_decoder) != 0 || !raw_decoder)
    {
        LOG(ERROR) << "WelsCreateDecoder failed";
        return false;
    }
    decoder_.reset(raw_decoder);

    SDecodingParam param;
    memset(&param, 0, sizeof(param));
    // 0xFF means "decode all spatial layers" - we never send SVC, so it just decodes the base.
    param.uiTargetDqLayer = static_cast<unsigned char>(-1);
    param.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;

    if (decoder_->Initialize(&param) != cmResultSuccess)
    {
        LOG(ERROR) << "OpenH264 decoder Initialize failed";
        decoder_.reset();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264SW::decode(const proto::video::Packet& packet, Frame* frame)
{
    if (!decoder_ || !frame)
        return false;

    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return false;
    }

    uint8_t* planes[3] = { nullptr, nullptr, nullptr };
    SBufferInfo info;
    memset(&info, 0, sizeof(info));

    DECODING_STATE state = decoder_->DecodeFrameNoDelay(
        reinterpret_cast<const uint8_t*>(packet.data().data()),
        static_cast<int>(packet.data().size()),
        planes, &info);
    if (state != dsErrorFree)
    {
        LOG(ERROR) << "DecodeFrameNoDelay failed:" << state;
        return false;
    }

    if (info.iBufferStatus != 1 || !planes[0] || !planes[1] || !planes[2])
    {
        // Decoder buffered the bitstream but did not emit a complete frame yet (warm-up after
        // a key frame is reasonably common). The caller will retry on the next packet.
        return false;
    }

    const int y_stride = info.UsrData.sSystemBuffer.iStride[0];
    const int uv_stride = info.UsrData.sSystemBuffer.iStride[1];
    const QSize decoded_size(info.UsrData.sSystemBuffer.iWidth, info.UsrData.sSystemBuffer.iHeight);

    if (decoded_size != frame->size())
    {
        LOG(ERROR) << "Size of the decoded frame" << decoded_size << "doesn't match" << frame->size();
        return false;
    }

    QRect frame_rect(QPoint(0, 0), frame->size());

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = parse(packet.dirty_rect(i));
        if (!frame_rect.contains(rect))
        {
            LOG(ERROR) << "The rectangle is outside the screen area";
            return false;
        }

        const int y_offset = rect.y() * y_stride + rect.x();
        const int uv_offset = (rect.y() / 2) * uv_stride + (rect.x() / 2);

        libyuv::I420ToARGB(planes[0] + y_offset, y_stride,
                           planes[1] + uv_offset, uv_stride,
                           planes[2] + uv_offset, uv_stride,
                           frame->frameDataAtPos(rect.topLeft()), frame->stride(),
                           rect.width(), rect.height());
    }

    return true;
}
