/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_vp8.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_vp8.h"

VideoDecoderVP8::VideoDecoderVP8() :
    last_image_(nullptr),
    bytes_per_pixel_(0),
    bytes_per_row_(0)
{
}

VideoDecoderVP8::~VideoDecoderVP8() {}

bool VideoDecoderVP8::PrepareResources(const DesktopSize &desktop_size,
                                       const PixelFormat &pixel_format)
{
    if (current_desktop_size_ != desktop_size ||
        current_pixel_format_ != pixel_format)
    {
        codec_.reset(new vpx_codec_ctx_t());

        current_desktop_size_ = desktop_size;
        current_pixel_format_ = pixel_format;

        bytes_per_pixel_ = current_pixel_format_.bytes_per_pixel();
        bytes_per_row_ = bytes_per_pixel_ * current_desktop_size_.width();

        vpx_codec_dec_cfg_t config;

        config.w = 0;
        config.h = 0;
        config.threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;

        vpx_codec_err_t ret = vpx_codec_dec_init(codec_.get(), vpx_codec_vp8_dx(), &config, 0);
        if (ret != VPX_CODEC_OK)
        {
            LOG(ERROR) << "vpx_codec_dec_init() failed: " <<
                vpx_codec_err_to_string(ret) << " (" << ret << ")";
            return false;
        }
    }

    return true;
}

bool VideoDecoderVP8::ConvertImageToARGB(const proto::VideoPacket *packet, uint8_t *dst)
{
    switch (last_image_->fmt)
    {
        case VPX_IMG_FMT_I420:
        {
            uint8_t *y_data = last_image_->planes[0];
            uint8_t *u_data = last_image_->planes[1];
            uint8_t *v_data = last_image_->planes[2];
            int y_stride = last_image_->stride[0];
            int uv_stride = last_image_->stride[1];

            switch (bytes_per_pixel_)
            {
                case 4:
                {
                    for (int i = 0; i < packet->changed_rect_size(); ++i)
                    {
                        const proto::VideoRect &rect = packet->changed_rect(i);

                        if (rect.x() + rect.width() > current_desktop_size_.width() ||
                            rect.y() + rect.height() > current_desktop_size_.height())
                        {
                            return false;
                        }

                        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * sizeof(uint32_t);
                        int y_offset = y_stride * rect.y() + rect.x();
                        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                        libyuv::I420ToARGB(y_data + y_offset, y_stride,
                                           u_data + uv_offset, uv_stride,
                                           v_data + uv_offset, uv_stride,
                                           dst + rgb_offset,
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

                        if (rect.x() + rect.width() > current_desktop_size_.width() ||
                            rect.y() + rect.height() > current_desktop_size_.height())
                        {
                            return false;
                        }

                        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * sizeof(uint16_t);
                        int y_offset = y_stride * rect.y() + rect.x();
                        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                        libyuv::I420ToRGB565(y_data + y_offset, y_stride,
                                             u_data + uv_offset, uv_stride,
                                             v_data + uv_offset, uv_stride,
                                             dst + rgb_offset,
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
            LOG(ERROR) << "Unsupported image format: " << last_image_->fmt;
            return false;
        }
    }

    return true;
}

bool VideoDecoderVP8::Decode(const proto::VideoPacket *packet,
                             const PixelFormat &dst_format,
                             uint8_t *dst)
{
    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        const proto::VideoPacketFormat &format = packet->format();

        DesktopSize size(format.screen_width(), format.screen_height());

        if (!PrepareResources(size, dst_format))
            return false;
    }

    vpx_codec_err_t ret;

    // Do the actual decoding.
    ret = vpx_codec_decode(codec_.get(),
                           reinterpret_cast<const uint8_t*>(packet->data().data()),
                           packet->data().size(),
                           nullptr, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_decode() failed: " <<
            vpx_codec_error(codec_.get()) << " (" << vpx_codec_error_detail(codec_.get()) << ")";
        return false;
    }

    vpx_codec_iter_t iter = nullptr;
    vpx_image_t *image;

    // Gets the decoded data.
    image = vpx_codec_get_frame(codec_.get(), &iter);
    if (!image)
    {
        LOG(ERROR) << "No video frame decoded";
        return false;
    }

    last_image_ = image;

    if (!ConvertImageToARGB(packet, dst))
    {
        LOG(ERROR) << "ConvertImageToARGB() failed";
        return false;
    }

    return true;
}
