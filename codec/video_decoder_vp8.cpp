/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_vp8.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_vp8.h"

#include <thread>

#include "libyuv/convert_from.h"
#include "base/logging.h"

VideoDecoderVP8::VideoDecoderVP8() :
    bytes_per_pixel_(0),
    bytes_per_row_(0)
{
    // Nothing
}

VideoDecoderVP8::~VideoDecoderVP8()
{
    // Nothing
}

void VideoDecoderVP8::ConvertImageToARGB(const proto::VideoPacket *packet, vpx_image_t *image, uint8_t *screen_buffer)
{
    switch (image->fmt)
    {
        case VPX_IMG_FMT_I420:
        {
            uint8_t *y_data = image->planes[0];
            uint8_t *u_data = image->planes[1];
            uint8_t *v_data = image->planes[2];
            int y_stride = image->stride[0];
            int uv_stride = image->stride[1];

            switch (bytes_per_pixel_)
            {
                case 4:
                {
                    for (int i = 0; i < packet->changed_rect_size(); ++i)
                    {
                        const proto::VideoRect &rect = packet->changed_rect(i);

                        if (rect.x() + rect.width() > screen_size_.width() ||
                            rect.y() + rect.height() > screen_size_.height())
                        {
                            break;
                        }

                        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * sizeof(uint32_t);
                        int y_offset = y_stride * rect.y() + rect.x();
                        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                        libyuv::I420ToARGB(y_data + y_offset, y_stride,
                                           u_data + uv_offset, uv_stride,
                                           v_data + uv_offset, uv_stride,
                                           screen_buffer + rgb_offset,
                                           bytes_per_row_,
                                           rect.width(),
                                           rect.height());
                    }
                }
                break;

                case 2:
                {
                    for (int i = 0; i < packet->changed_rect_size(); ++i)
                    {
                        const proto::VideoRect &rect = packet->changed_rect(i);

                        if (rect.x() + rect.width() > screen_size_.width() ||
                            rect.y() + rect.height() > screen_size_.height())
                        {
                            break;
                        }

                        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * sizeof(uint16_t);
                        int y_offset = y_stride * rect.y() + rect.x();
                        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                        libyuv::I420ToRGB565(y_data + y_offset, y_stride,
                                             u_data + uv_offset, uv_stride,
                                             v_data + uv_offset, uv_stride,
                                             screen_buffer + rgb_offset,
                                             bytes_per_row_,
                                             rect.width(),
                                             rect.height());
                    }
                }
                break;
            }
        }
        break;

        default:
        {
            DLOG(ERROR) << "Unsupported image format: " << image->fmt;
        }
    }
}

void VideoDecoderVP8::Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format)
{
    codec_.reset(new vpx_codec_ctx_t());

    screen_size_ = screen_size;

    bytes_per_pixel_ = pixel_format.bytes_per_pixel();
    bytes_per_row_ = bytes_per_pixel_ * screen_size.width();

    vpx_codec_dec_cfg_t config;

    config.w = 0;
    config.h = 0;
    config.threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;

    vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), vpx_codec_vp8_dx(), &config, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_dec_init() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
    }
}

void VideoDecoderVP8::Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer)
{
    // Do the actual decoding.
    vpx_codec_err_t ret =
        vpx_codec_decode(codec_.get(),
                         reinterpret_cast<const uint8_t*>(packet->data().data()),
                         packet->data().size(),
                         nullptr,
                         0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_decode() failed: " <<
            vpx_codec_error(codec_.get()) << " (" << vpx_codec_error_detail(codec_.get()) << ")";
        return;
    }

    vpx_codec_iter_t iter = nullptr;

    // Gets the decoded data.
    vpx_image_t *image = vpx_codec_get_frame(codec_.get(), &iter);
    if (!image)
    {
        LOG(ERROR) << "No video frame decoded";
        return;
    }

    ConvertImageToARGB(packet, image, screen_buffer);
}
