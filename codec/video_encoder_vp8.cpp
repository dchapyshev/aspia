/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/vodeo_encoder_vp8.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_encoder_vp8.h"

static const int kBlockSize = 16;

VideoEncoderVP8::VideoEncoderVP8() :
    codec_(nullptr),
    active_map_size_(0),
    bytes_per_row_(0),
    bytes_per_pixel_(0),
    last_timestamp_(0)
{
    memset(&active_map_, 0, sizeof(active_map_));
}

VideoEncoderVP8::~VideoEncoderVP8()
{
}

void VideoEncoderVP8::CreateImage()
{
    memset(&image_, 0, sizeof(vpx_image_t));

    image_.d_w = image_.w = current_desktop_size_.width();
    image_.d_h = image_.h = current_desktop_size_.height();

    image_.fmt = VPX_IMG_FMT_YV12;
    image_.x_chroma_shift = 1;
    image_.y_chroma_shift = 1;

    //
    // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad
    // the Y, U and V planes' strides to multiples of 16 bytes.
    //
    int y_stride = ((image_.w - 1) & ~31) + 32;
    int uv_unaligned_stride = y_stride >> image_.x_chroma_shift;
    int uv_stride = ((uv_unaligned_stride - 1) & ~31) + 32;

    //
    // libvpx accesses the source image in macro blocks, and will over-read
    // if the image is not padded out to the next macroblock: crbug.com/119633.
    // Pad the Y, U and V planes' height out to compensate.
    // Assuming macroblocks are 16x16, aligning the planes' strides above also
    // macroblock aligned them.
    //
    int y_rows = ((image_.h - 1) & ~(kBlockSize - 1)) + kBlockSize;
    int uv_rows = y_rows >> image_.y_chroma_shift;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    int buffer_size = y_stride * y_rows + (2 * uv_stride) * uv_rows;

    yuv_image_.reset(new uint8_t[buffer_size]);

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(yuv_image_.get(), 128, buffer_size);

    // Fill in the information
    image_.planes[0] = yuv_image_.get();
    image_.planes[1] = image_.planes[0] + y_stride * y_rows;
    image_.planes[2] = image_.planes[1] + uv_stride * uv_rows;

    image_.stride[0] = y_stride; //-V525
    image_.stride[1] = uv_stride;
    image_.stride[2] = uv_stride;
}

void VideoEncoderVP8::CreateActiveMap()
{
    active_map_.cols = (current_desktop_size_.width() + kBlockSize - 1) / kBlockSize;
    active_map_.rows = (current_desktop_size_.height() + kBlockSize - 1) / kBlockSize;

    active_map_size_ = active_map_.cols * active_map_.rows;

    active_map_buffer_.reset(new uint8_t[active_map_size_]);

    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVP8::SetCommonCodecParameters(vpx_codec_enc_cfg_t *config)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = 20;

    // Adjust default target bit-rate to account for actual desktop size.
    config->rc_target_bitrate = current_desktop_size_.width() * current_desktop_size_.height() *
        config->rc_target_bitrate / config->g_w / config->g_h;

    config->g_w = current_desktop_size_.width();
    config->g_h = current_desktop_size_.height();
    config->g_pass = VPX_RC_ONE_PASS;

    // Start emitting packets immediately.
    config->g_lag_in_frames = 0;

    //
    // Using 2 threads gives a great boost in performance for most systems with
    // adequate processing power. NB: Going to multiple threads on low end
    // windows systems can really hurt performance.
    // http://crbug.com/99179
    //
    config->g_threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;
}

bool VideoEncoderVP8::CreateCodec()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config;
    vpx_codec_iface_t *algo;
    vpx_codec_err_t ret;

    // Configure the encoder.
    algo = vpx_codec_vp8_cx();
    ret = vpx_codec_enc_config_default(algo, &config, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_default() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        return false;
    }

    SetCommonCodecParameters(&config);

    //
    // Value of 2 means using the real time profile. This is basically a
    // redundant option since we explicitly select real time mode when doing
    // encoding.
    //
    config.g_profile = 2;

    // Clamping the quantizer constrains the worst-case quality and CPU usage.
    config.rc_min_quantizer = 20;
    config.rc_max_quantizer = 30;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_init() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        return false;
    }

    // Value of 16 will have the smallest CPU load. This turns off subpixel motion search.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        return false;
    }

    ret = vpx_codec_control(codec_.get(), VP8E_SET_SCREEN_CONTENT_MODE, 1);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        return false;
    }

    //
    // Use the lowest level of noise sensitivity so as to spend less time
    // on motion estimation and inter-prediction mode.
    //
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control() failed: " <<
            vpx_codec_err_to_string(ret) << " (" << ret << ")";
        return false;
    }

    return true;
}

void VideoEncoderVP8::PrepareCodec(const DesktopSize &desktop_size, int bytes_per_pixel)
{
    if (current_desktop_size_ != desktop_size || bytes_per_pixel_ != bytes_per_pixel)
    {
        current_desktop_size_ = desktop_size;
        bytes_per_pixel_ = bytes_per_pixel;
        bytes_per_row_ = bytes_per_pixel * desktop_size.width();

        codec_.reset();
    }

    if (!codec_)
    {
        CreateImage();
        CreateActiveMap();
        CreateCodec();
    }
}

void VideoEncoderVP8::PrepareImageAndActiveMap(const DesktopRegion &region,
                                               const uint8_t *src,
                                               proto::VideoPacket *packet)
{
    memset(active_map_.active_map, 0, active_map_size_);

    int y_stride = image_.stride[0];
    int uv_stride = image_.stride[1];
    uint8_t *y_data = image_.planes[0];
    uint8_t *u_data = image_.planes[1];
    uint8_t *v_data = image_.planes[2];

    for (DesktopRegion::Iterator iter(region); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect &rect = iter.rect();

        int rgb_offset = bytes_per_row_ * rect.top() + rect.left() * bytes_per_pixel_;
        int y_offset = y_stride * rect.top() + rect.left();
        int uv_offset = uv_stride * rect.top() / 2 + rect.left() / 2;
        int width = rect.width();
        int height = rect.height();

        switch (bytes_per_pixel_)
        {
            case 4:
            {
                libyuv::ARGBToI420(src + rgb_offset, bytes_per_row_,
                                   y_data + y_offset, y_stride,
                                   u_data + uv_offset, uv_stride,
                                   v_data + uv_offset, uv_stride,
                                   width,
                                   height);
            }
            break;

            case 2:
            {
                libyuv::RGB565ToI420(src + rgb_offset, bytes_per_row_,
                                     y_data + y_offset, y_stride,
                                     u_data + uv_offset, uv_stride,
                                     v_data + uv_offset, uv_stride,
                                     width,
                                     height);
            }
            break;
        }

        proto::VideoRect *video_rect = packet->add_changed_rect();

        video_rect->set_x(rect.x());
        video_rect->set_y(rect.y());
        video_rect->set_width(width);
        video_rect->set_height(height);

        int left   = rect.left() / kBlockSize;
        int top    = rect.top() / kBlockSize;
        int right  = (rect.right() - 1) / kBlockSize;
        int bottom = (rect.bottom() - 1) / kBlockSize;

        uint8_t *map = active_map_.active_map + top * active_map_.cols;

        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                map[x] = 1;
            }

            map += active_map_.cols;
        }
    }
}

proto::VideoPacket* VideoEncoderVP8::Encode(const DesktopSize &desktop_size,
                                            const PixelFormat &src_format,
                                            const PixelFormat &dst_format,
                                            const DesktopRegion &changed_region,
                                            const uint8_t *src_buffer)
{
    PrepareCodec(desktop_size, src_format.bytes_per_pixel());

    proto::VideoPacket *packet = GetEmptyPacket();

    packet->set_flags(proto::VideoPacket::FIRST_PACKET | proto::VideoPacket::LAST_PACKET);

    proto::VideoPacketFormat *packet_format = packet->mutable_format();

    packet_format->set_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);

    packet_format->set_screen_width(current_desktop_size_.width());
    packet_format->set_screen_height(current_desktop_size_.height());

    //
    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    //
    PrepareImageAndActiveMap(changed_region, src_buffer, packet);

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "Unable to apply active map";
    }

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(), &image_, last_timestamp_, 1, 0, VPX_DL_REALTIME);

    DCHECK_EQ(ret, VPX_CODEC_OK)
        << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
        << "Details: " << vpx_codec_error(codec_.get()) << "\n"
        << vpx_codec_error_detail(codec_.get());

    // TODO(hclam): Apply the proper timestamp here.
    last_timestamp_ += 50;

    // Read the encoded data.
    vpx_codec_iter_t iter = nullptr;
    bool got_data = false;

    while (!got_data)
    {
        const vpx_codec_cx_pkt_t *vpx_packet =
            vpx_codec_get_cx_data(codec_.get(), &iter);

        if (!vpx_packet)
            continue;

        switch (vpx_packet->kind)
        {
            case VPX_CODEC_CX_FRAME_PKT:
            {
                got_data = true;
                // TODO(sergeyu): Split each frame into multiple partitions.
                packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
            }
            break;

            default:
            break;
        }
    }

    return packet;
}
