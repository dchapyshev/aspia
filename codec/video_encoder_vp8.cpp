/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/vodeo_encoder_vp8.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_encoder_vp8.h"

#include <thread>

#include "base/logging.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/convert.h"

namespace aspia {

static const int kBlockSize = 16;

VideoEncoderVP8::VideoEncoderVP8() :
    codec_(nullptr),
    active_map_size_(0),
    bytes_per_row_(0),
    bytes_per_pixel_(0),
    resized_(true)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

VideoEncoderVP8::~VideoEncoderVP8()
{
    // Nothing
}

void VideoEncoderVP8::CreateImage()
{
    memset(&image_, 0, sizeof(image_));

    image_.d_w = image_.w = screen_size_.width();
    image_.d_h = image_.h = screen_size_.height();

    image_.fmt = VPX_IMG_FMT_YV12;
    image_.x_chroma_shift = 1;
    image_.y_chroma_shift = 1;

    //
    // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad
    // the Y, U and V planes' strides to multiples of 16 bytes.
    //
    int y_stride = ((image_.w - 1) & ~15) + 16;
    int uv_unaligned_stride = y_stride >> image_.x_chroma_shift;
    int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

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
    active_map_.cols = (screen_size_.width() + kBlockSize - 1) / kBlockSize;
    active_map_.rows = (screen_size_.height() + kBlockSize - 1) / kBlockSize;

    active_map_size_ = active_map_.cols * active_map_.rows;

    active_map_buffer_.reset(new uint8_t[active_map_size_]);

    memset(active_map_buffer_.get(), 0, active_map_size_);

    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVP8::SetCommonCodecParameters(vpx_codec_enc_cfg_t *config)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = 1000;

    config->g_w = screen_size_.width();
    config->g_h = screen_size_.height();
    config->g_pass = VPX_RC_ONE_PASS;

    // Start emitting packets immediately.
    config->g_lag_in_frames = 0;

    // Since the transport layer is reliable, keyframes should not be necessary.
    // However, due to crbug.com/440223, decoding fails after 30,000 non-key
    // frames, so take the hit of an "unnecessary" key-frame every 10,000 frames.
    config->kf_min_dist = 10000;
    config->kf_max_dist = 10000;

    //
    // Using 2 threads gives a great boost in performance for most systems with
    // adequate processing power. NB: Going to multiple threads on low end
    // windows systems can really hurt performance.
    // http://crbug.com/99179
    //
    config->g_threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;
}

void VideoEncoderVP8::CreateCodec()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t *algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

    // Adjust default target bit-rate to account for actual desktop size.
    config.rc_target_bitrate = screen_size_.width() * screen_size_.height() *
        config.rc_target_bitrate / config.g_w / config.g_h;

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
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to initialize codec";

    // Value of 16 will have the smallest CPU load. This turns off subpixel motion search.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

    ret = vpx_codec_control(codec_.get(), VP8E_SET_SCREEN_CONTENT_MODE, 1);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set screen content mode";

    //
    // Use the lowest level of noise sensitivity so as to spend less time
    // on motion estimation and inter-prediction mode.
    //
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";
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

        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * bytes_per_pixel_;
        int y_offset = y_stride * rect.y() + rect.x();
        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;
        int width = rect.width();
        int height = rect.height();

        convert_image_func_(src + rgb_offset, bytes_per_row_,
                            y_data + y_offset, y_stride,
                            u_data + uv_offset, uv_stride,
                            v_data + uv_offset, uv_stride,
                            width,
                            height);

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

void VideoEncoderVP8::Resize(const DesktopSize &screen_size,
                             const PixelFormat &host_pixel_format,
                             const PixelFormat &client_pixel_format)
{
    screen_size_ = screen_size;

    bytes_per_pixel_ = host_pixel_format.BytesPerPixel();
    bytes_per_row_ = bytes_per_pixel_ * screen_size.width();

    CreateImage();
    CreateActiveMap();
    CreateCodec();

    switch (bytes_per_pixel_)
    {
        case 4: convert_image_func_ = libyuv::ARGBToI420;   break;
        case 2: convert_image_func_ = libyuv::RGB565ToI420; break;
    }

    CHECK(convert_image_func_);

    resized_ = true;
}

int32_t VideoEncoderVP8::Encode(proto::VideoPacket *packet,
                                const uint8_t *screen_buffer,
                                const DesktopRegion &changed_region)
{
    packet->set_flags(proto::VideoPacket::FIRST_PACKET | proto::VideoPacket::LAST_PACKET);

    proto::VideoPacketFormat *format = packet->mutable_format();

    format->set_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);

    if (resized_)
    {
        proto::VideoSize *size = format->mutable_screen_size();

        size->set_width(screen_size_.width());
        size->set_height(screen_size_.height());

        resized_ = false;
    }

    //
    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    //
    PrepareImageAndActiveMap(changed_region, screen_buffer, packet);

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    DCHECK_EQ(ret, VPX_CODEC_OK) << "Unable to apply active map";

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(), &image_, 0, 1, 0, VPX_DL_REALTIME);

    DCHECK_EQ(ret, VPX_CODEC_OK)
        << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
        << "Details: " << vpx_codec_error(codec_.get()) << "\n"
        << vpx_codec_error_detail(codec_.get());

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
                packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
            }
            break;

            default:
            break;
        }
    }

    return packet->flags();
}

} // namespace aspia
