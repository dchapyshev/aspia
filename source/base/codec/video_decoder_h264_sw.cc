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
#include "proto/desktop_video.h"

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
    param.eEcActiveIdc = ERROR_CON_FRAME_COPY;
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
VideoDecoder::Result VideoDecoderH264SW::decode(const proto::video::Packet& packet)
{
    if (!decoder_)
        return Result::PERMANENT_ERROR;

    const QSize size = frameSize(packet);
    if (size.isEmpty())
    {
        LOG(ERROR) << "Unknown frame size";
        return Result::TEMPORARY_ERROR;
    }

    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return Result::TEMPORARY_ERROR;
    }

    uint8_t* planes[3] = { nullptr, nullptr, nullptr };
    SBufferInfo info;
    memset(&info, 0, sizeof(info));

    DECODING_STATE state = decoder_->DecodeFrame2(
        reinterpret_cast<const uint8_t*>(packet.data().data()),
        static_cast<int>(packet.data().size()),
        planes, &info);

    // DecodeFrame2 may buffer the bitstream without producing output on the first call (typical
    // warm-up before SPS/PPS arrive). Pump it once more with a null input to drain.
    if (info.iBufferStatus != 1 && (state == dsErrorFree || state == dsNoParamSets))
        state = decoder_->DecodeFrame2(nullptr, 0, planes, &info);

    if (state != dsErrorFree)
    {
        LOG(ERROR) << "DecodeFrame2 failed:" << state;
        return Result::TEMPORARY_ERROR;
    }

    if (info.iBufferStatus != 1)
        return Result::TEMPORARY_ERROR;

    // FlushFrame finalizes the current decoded frame so the planes pointers contain ready data.
    state = decoder_->FlushFrame(planes, &info);
    if (state != dsErrorFree)
    {
        LOG(ERROR) << "FlushFrame failed:" << state;
        return Result::TEMPORARY_ERROR;
    }

    if (!planes[0] || !planes[1] || !planes[2])
        return Result::TEMPORARY_ERROR;

    const int y_stride = info.UsrData.sSystemBuffer.iStride[0];
    const int uv_stride = info.UsrData.sSystemBuffer.iStride[1];

    // OpenH264 reports the coded buffer size (rounded up to a 16-pixel macroblock boundary), so
    // anything that covers the display size is fine; the dirty rects are within display bounds.
    if (info.UsrData.sSystemBuffer.iWidth < size.width() ||
        info.UsrData.sSystemBuffer.iHeight < size.height())
    {
        LOG(ERROR) << "Decoded frame is smaller than expected" << size;
        return Result::TEMPORARY_ERROR;
    }

    if (info.UsrData.sSystemBuffer.iFormat != videoFormatI420)
    {
        LOG(ERROR) << "Unexpected pixel format from OpenH264:"
                   << info.UsrData.sSystemBuffer.iFormat;
        return Result::TEMPORARY_ERROR;
    }

    frame_.reset(YuvFormat::I420, size);
    frame_.setPlane(0, planes[0], y_stride);
    frame_.setPlane(1, planes[1], uv_stride);
    frame_.setPlane(2, planes[2], uv_stride);

    return Result::SUCCESS;
}
