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

#include "base/codec/video_decoder_vpx.h"

#include <QThread>

#include "base/logging.h"
#include "proto/desktop_video.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

//--------------------------------------------------------------------------------------------------
VideoDecoderVpx::VideoDecoderVpx(proto::video::Encoding encoding)
{
    const int cpu_count = std::max(1, QThread::idealThreadCount());
    quint32 thread_count;
    vpx_codec_iface_t* algo;

    switch (encoding)
    {
        case proto::video::ENCODING_VP8:
            // VP8 does not support tile-based decoding, multithreading is only used for loop
            // filter, so more than 2 threads provide no benefit.
            thread_count = (cpu_count >= 4) ? 2 : 1;
            algo = vpx_codec_vp8_dx();
            break;

        case proto::video::ENCODING_VP9:
            // VP9 supports tile-based parallel decoding. Use half of the available cores,
            // clamped to [2, 8] range to balance performance and CPU usage.
            thread_count = std::clamp(cpu_count / 2, 2, 8);
            algo = vpx_codec_vp9_dx();
            break;

        default:
            LOG(FATAL) << "Unsupported video encoding:" << encoding;
            return;
    }

    LOG(INFO) << "VPX(" << encoding << ") Ctor (thread_count=" << thread_count << ")";
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_dec_cfg_t config;
    config.w = 0;
    config.h = 0;
    config.threads = thread_count;

    int ret = vpx_codec_dec_init(codec_.get(), algo, &config, 0);
    CHECK_EQ(ret, VPX_CODEC_OK);
}

//--------------------------------------------------------------------------------------------------
VideoDecoderVpx::~VideoDecoderVpx()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
VideoDecoder::Result VideoDecoderVpx::decode(const proto::video::Packet& packet)
{
    const QSize size = frameSize(packet);
    if (size.isEmpty())
    {
        LOG(ERROR) << "Unknown frame size";
        return Result::TEMPORARY_ERROR;
    }

    // Do the actual decoding.
    vpx_codec_err_t ret =
        vpx_codec_decode(codec_.get(),
                         reinterpret_cast<const quint8*>(packet.data().data()),
                         static_cast<unsigned int>(packet.data().size()),
                         nullptr,
                         0);
    if (ret != VPX_CODEC_OK)
    {
        const char* error = vpx_codec_error(codec_.get());
        const char* error_detail = vpx_codec_error_detail(codec_.get());

        LOG(ERROR) << "Decoding failed:" << (error ? error : "(NULL)") << "\n"
                   << "Details:" << (error_detail ? error_detail : "(NULL)");
        return Result::TEMPORARY_ERROR;
    }

    vpx_codec_iter_t iter = nullptr;

    // Gets the decoded data.
    vpx_image_t* image = vpx_codec_get_frame(codec_.get(), &iter);
    if (!image)
    {
        LOG(ERROR) << "No video frame decoded";
        return Result::TEMPORARY_ERROR;
    }

    if (image->fmt != VPX_IMG_FMT_I420)
    {
        LOG(ERROR) << "Unexpected pixel format from libvpx:" << image->fmt;
        return Result::TEMPORARY_ERROR;
    }

    if (QSize(static_cast<int>(image->d_w), static_cast<int>(image->d_h)) != size)
    {
        LOG(ERROR) << "Size of the encoded frame doesn't match size in the header";
        return Result::TEMPORARY_ERROR;
    }

    frame_.reset(YuvFormat::I420, size);
    frame_.setPlane(0, image->planes[0], image->stride[0]);
    frame_.setPlane(1, image->planes[1], image->stride[1]);
    frame_.setPlane(2, image->planes[2], image->stride[2]);

    return Result::SUCCESS;
}
