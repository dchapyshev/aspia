//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_vp8.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_vp8.h"

#include "libyuv/convert_from.h"
#include "base/logging.h"

namespace aspia {

VideoDecoderVP8::VideoDecoderVP8()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_dec_cfg_t config;

    config.w = 0;
    config.h = 0;
    config.threads = 2;

    vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), vpx_codec_vp8_dx(), &config, 0);
    CHECK_EQ(VPX_CODEC_OK, ret);
}

bool VideoDecoderVP8::ConvertImage(const proto::VideoPacket& packet,
                                   vpx_image_t* image,
                                   DesktopFrame* frame)
{
    switch (image->fmt)
    {
        case VPX_IMG_FMT_I420:
        {
            uint8_t* y_data = image->planes[0];
            uint8_t* u_data = image->planes[1];
            uint8_t* v_data = image->planes[2];
            int y_stride = image->stride[0];
            int uv_stride = image->stride[1];

            for (int i = 0; i < packet.dirty_rect_size(); ++i)
            {
                const proto::VideoRect& rect = packet.dirty_rect(i);

                int y_offset = y_stride * rect.y() + rect.x();
                int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                libyuv::I420ToARGB(y_data + y_offset, y_stride,
                                   u_data + uv_offset, uv_stride,
                                   v_data + uv_offset, uv_stride,
                                   frame->GetFrameDataAtPos(rect.x(), rect.y()),
                                   frame->Stride(),
                                   rect.width(),
                                   rect.height());
            }
        }
        break;

        default:
        {
            LOG(ERROR) << "Unsupported image format: " << image->fmt;
            return false;
        }
    }

    return true;
}

bool VideoDecoderVP8::Decode(const proto::VideoPacket& packet, DesktopFrame* frame)
{
    // Do the actual decoding.
    vpx_codec_err_t ret =
        vpx_codec_decode(codec_.get(),
                         reinterpret_cast<const uint8_t*>(packet.data().data()),
                         packet.data().size(),
                         nullptr,
                         0);
    if (ret != VPX_CODEC_OK)
    {
        const char* error = vpx_codec_error(codec_.get());
        const char* error_detail = vpx_codec_error_detail(codec_.get());

        LOG(ERROR) << "Decoding failed:" << (error ? error : "(NULL)") << "\n"
                   << "Details: " << (error_detail ? error_detail : "(NULL)");
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

    if (!ConvertImage(packet, image, frame))
        return false;

    return true;
}

} // namespace aspia
