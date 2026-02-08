//
// SmartCafe Project
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

#include "base/codec/video_decoder_vpx.h"

#include <QThread>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/frame.h"

#include <libyuv/convert_from.h>
#include <libyuv/convert_argb.h>

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool convertImage(const proto::desktop::VideoPacket& packet, vpx_image_t* image, Frame* frame)
{
    if (image->fmt != VPX_IMG_FMT_I420)
        return false;

    QRect frame_rect = QRect(QPoint(0, 0), frame->size());

    quint8* y_data = image->planes[0];
    quint8* u_data = image->planes[1];
    quint8* v_data = image->planes[2];

    int y_stride = image->stride[0];
    int uv_stride = image->stride[1];

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = base::parse(packet.dirty_rect(i));
        if (!frame_rect.contains(rect))
        {
            LOG(ERROR) << "The rectangle is outside the screen area";
            return false;
        }

        int y_offset = y_stride * rect.y() + rect.x();
        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

        libyuv::I420ToARGB(y_data + y_offset, y_stride,
                           u_data + uv_offset, uv_stride,
                           v_data + uv_offset, uv_stride,
                           frame->frameDataAtPos(rect.topLeft()),
                           frame->stride(),
                           rect.width(),
                           rect.height());
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderVPX> VideoDecoderVPX::createVP8()
{
    return std::unique_ptr<VideoDecoderVPX>(new VideoDecoderVPX(proto::desktop::VIDEO_ENCODING_VP8));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderVPX> VideoDecoderVPX::createVP9()
{
    return std::unique_ptr<VideoDecoderVPX>(new VideoDecoderVPX(proto::desktop::VIDEO_ENCODING_VP9));
}

//--------------------------------------------------------------------------------------------------
VideoDecoderVPX::VideoDecoderVPX(proto::desktop::VideoEncoding encoding)
{
    quint32 thread_count = QThread::idealThreadCount();
    if (thread_count >= 8)
        thread_count = 4;
    else if (thread_count >= 4)
        thread_count = 2;
    else
        thread_count = 1;

    LOG(INFO) << "VPX(" << encoding << ") Ctor (thread_count=" << thread_count << ")";
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_dec_cfg_t config;
    config.w = 0;
    config.h = 0;
    config.threads = thread_count;

    vpx_codec_iface_t* algo;

    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            algo = vpx_codec_vp8_dx();
            break;

        case proto::desktop::VIDEO_ENCODING_VP9:
            algo = vpx_codec_vp9_dx();
            break;

        default:
            LOG(FATAL) << "Unsupported video encoding:" << encoding;
            return;
    }

    int ret = vpx_codec_dec_init(codec_.get(), algo, &config, 0);
    CHECK_EQ(ret, VPX_CODEC_OK);
}

//--------------------------------------------------------------------------------------------------
VideoDecoderVPX::~VideoDecoderVPX()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderVPX::decode(const proto::desktop::VideoPacket& packet, Frame* frame)
{
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
        return false;
    }

    vpx_codec_iter_t iter = nullptr;

    // Gets the decoded data.
    vpx_image_t* image = vpx_codec_get_frame(codec_.get(), &iter);
    if (!image)
    {
        LOG(ERROR) << "No video frame decoded";
        return false;
    }

    if (QSize(static_cast<int>(image->d_w), static_cast<int>(image->d_h)) != frame->size())
    {
        LOG(ERROR) << "Size of the encoded frame doesn't match size in the header";
        return false;
    }

    return convertImage(packet, image, frame);
}

} // namespace base
