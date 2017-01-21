//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/vodeo_encoder_vp9.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_vp9.h"

#include <thread>

#include "base/logging.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/convert.h"

namespace aspia {

static const int kBytesPerPixel = 4;

//
// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
//
static const int kBlockSize = 16;

// Magic encoder profile numbers for I444 input formats.
static const int kVp9I444ProfileNumber = 1;

// Magic encoder constants for adaptive quantization strategy.
static const int kVp9AqModeNone = 0;

VideoEncoderVP9::VideoEncoderVP9() :
    codec_(nullptr),
    active_map_size_(0),
    bytes_per_row_(0),
    resized_(true)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

VideoEncoderVP9::~VideoEncoderVP9()
{
    // Nothing
}

void VideoEncoderVP9::CreateImage()
{
    memset(&image_, 0, sizeof(image_));

    image_.d_w = image_.w = screen_size_.Width();
    image_.d_h = image_.h = screen_size_.Height();

    image_.fmt = VPX_IMG_FMT_I444;
    image_.x_chroma_shift = 0;
    image_.y_chroma_shift = 0;

    //
    // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad
    // the Y, U and V planes' strides to multiples of 16 bytes.
    //
    const int y_stride = ((image_.w - 1) & ~15) + 16;
    const int uv_unaligned_stride = y_stride >> image_.x_chroma_shift;
    const int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

    //
    // libvpx accesses the source image in macro blocks, and will over-read
    // if the image is not padded out to the next macroblock: crbug.com/119633.
    // Pad the Y, U and V planes' height out to compensate.
    // Assuming macroblocks are 16x16, aligning the planes' strides above also
    // macroblock aligned them.
    //
    const int y_rows = ((image_.h - 1) & ~(kBlockSize - 1)) + kBlockSize;
    const int uv_rows = y_rows >> image_.y_chroma_shift;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    const int buffer_size = y_stride * y_rows + (2 * uv_stride) * uv_rows;

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

void VideoEncoderVP9::CreateActiveMap()
{
    active_map_.cols = (screen_size_.Width() + kBlockSize - 1) / kBlockSize;
    active_map_.rows = (screen_size_.Height() + kBlockSize - 1) / kBlockSize;

    active_map_size_ = active_map_.cols * active_map_.rows;

    active_map_buffer_.reset(new uint8_t[active_map_size_]);

    memset(active_map_buffer_.get(), 0, active_map_size_);

    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVP9::SetCommonCodecParameters(vpx_codec_enc_cfg_t *config)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = 1000;

    config->g_w = screen_size_.Width();
    config->g_h = screen_size_.Height();
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

void VideoEncoderVP9::CreateCodec()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t *algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

    SetCommonCodecParameters(&config);

    // Configure VP9 for I444 source frames.
    config.g_profile = kVp9I444ProfileNumber;

    // Disable quantization entirely, putting the encoder in "lossless" mode.
    config.rc_min_quantizer = 0;
    config.rc_max_quantizer = 0;
    config.rc_end_usage = VPX_VBR;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to initialize codec";

    //
    // Request the lowest-CPU usage that VP9 supports, which depends on whether
    // we are encoding lossy or lossless.
    //
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 5);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set CPUUSED";

    ret = vpx_codec_control(codec_.get(), VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set screen content mode";

    //
    // Use the lowest level of noise sensitivity so as to spend less time
    // on motion estimation and inter-prediction mode.
    //
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set noise sensitivity";

    // Set cyclic refresh (aka "top-off") only for lossy encoding.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_AQ_MODE, kVp9AqModeNone);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to set aq mode";
}

void VideoEncoderVP9::PrepareImageAndActiveMap(const DesktopRegion &dirty_region,
                                               const uint8_t *src,
                                               proto::VideoPacket *packet)
{
    DCHECK(image_.fmt == VPX_IMG_FMT_I444);

    memset(active_map_.active_map, 0, active_map_size_);

    int y_stride = image_.stride[0];
    int uv_stride = image_.stride[1];
    uint8_t *y_data = image_.planes[0];
    uint8_t *u_data = image_.planes[1];
    uint8_t *v_data = image_.planes[2];

    for (DesktopRegion::Iterator iter(dirty_region); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect &rect = iter.rect();

        int rgb_offset = bytes_per_row_ * rect.y() + rect.x() * kBytesPerPixel;
        int yuv_offset = uv_stride * rect.y() + rect.x();
        int width = rect.Width();
        int height = rect.Height();

        libyuv::ARGBToI444(src + rgb_offset, bytes_per_row_,
                           y_data + yuv_offset, y_stride,
                           u_data + yuv_offset, uv_stride,
                           v_data + yuv_offset, uv_stride,
                           width,
                           height);

        int left   = rect.Left() / kBlockSize;
        int top    = rect.Top() / kBlockSize;
        int right  = (rect.Right() - 1) / kBlockSize;
        int bottom = (rect.Bottom() - 1) / kBlockSize;

        uint8_t *map = active_map_.active_map + top * active_map_.cols;

        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                map[x] = 1;
            }

            map += active_map_.cols;
        }

        rect.ToVideoRect(packet->add_dirty_rect());
    }
}

void VideoEncoderVP9::Resize(const DesktopSize &screen_size,
                             const PixelFormat &client_pixel_format)
{
    screen_size_ = screen_size;

    bytes_per_row_ = kBytesPerPixel * screen_size.Width();

    CreateImage();
    CreateActiveMap();
    CreateCodec();

    resized_ = true;
}

int32_t VideoEncoderVP9::Encode(proto::VideoPacket *packet,
                                const uint8_t *screen_buffer,
                                const DesktopRegion &dirty_region)
{
    packet->set_flags(proto::VideoPacket::FIRST_PACKET | proto::VideoPacket::LAST_PACKET);

    proto::VideoPacketFormat *format = packet->mutable_format();

    format->set_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP9);

    if (resized_)
    {
        screen_size_.ToVideoSize(format->mutable_screen_size());
        resized_ = false;
    }

    //
    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    //
    PrepareImageAndActiveMap(dirty_region, screen_buffer, packet);

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
