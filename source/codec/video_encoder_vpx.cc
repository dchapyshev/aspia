//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/vodeo_encoder_vpx.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_vpx.h"
#include "codec/video_helpers.h"
#include <libyuv/convert_from_argb.h>
#include "base/logging.h"

#include <thread>

namespace aspia {

//
// Defines the dimension of a macro block. This is used to compute the active
// map for the encoder.
//
static const int kMacroBlockSize = 16;

// Magic encoder profile numbers for I444 input formats.
static const int kVp9I444ProfileNumber = 1;

// Magic encoder constants for adaptive quantization strategy.
static const int kVp9AqModeNone = 0;

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::CreateVP8()
{
    return std::unique_ptr<VideoEncoderVPX>(
        new VideoEncoderVPX(proto::VideoEncoding::VIDEO_ENCODING_VP8));
}

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::CreateVP9()
{
    return std::unique_ptr<VideoEncoderVPX>(
        new VideoEncoderVPX(proto::VideoEncoding::VIDEO_ENCODING_VP9));
}

VideoEncoderVPX::VideoEncoderVPX(proto::VideoEncoding encoding)
    : encoding_(encoding)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

void VideoEncoderVPX::CreateImage()
{
    memset(&image_, 0, sizeof(image_));

    image_.d_w = image_.w = screen_size_.Width();
    image_.d_h = image_.h = screen_size_.Height();

    if (encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP8)
    {
        image_.fmt = VPX_IMG_FMT_YV12;
        image_.x_chroma_shift = 1;
        image_.y_chroma_shift = 1;
    }
    else if (encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP9)
    {
        image_.fmt = VPX_IMG_FMT_I444;
        image_.x_chroma_shift = 0;
        image_.y_chroma_shift = 0;
    }

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
    const int y_rows =
        ((image_.h - 1) & ~(kMacroBlockSize - 1)) + kMacroBlockSize;

    const int uv_rows = y_rows >> image_.y_chroma_shift;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    const int buffer_size = y_stride * y_rows + (2 * uv_stride) * uv_rows;

    yuv_image_ = std::make_unique<uint8_t[]>(buffer_size);

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(yuv_image_.get(), 128, buffer_size);

    // Fill in the information
    image_.planes[0] = yuv_image_.get();
    image_.planes[1] = image_.planes[0] + y_stride * y_rows;
    image_.planes[2] = image_.planes[1] + uv_stride * uv_rows;

    image_.stride[0] = y_stride;
    image_.stride[1] = image_.stride[2] = uv_stride;
}

void VideoEncoderVPX::CreateActiveMap()
{
    active_map_.cols =
        (screen_size_.Width() + kMacroBlockSize - 1) / kMacroBlockSize;

    active_map_.rows =
        (screen_size_.Height() + kMacroBlockSize - 1) / kMacroBlockSize;

    active_map_size_ = active_map_.cols * active_map_.rows;

    active_map_buffer_ = std::make_unique<uint8_t[]>(active_map_size_);

    memset(active_map_buffer_.get(), 0, active_map_size_);

    active_map_.active_map = active_map_buffer_.get();
}

static void SetCommonCodecParameters(vpx_codec_enc_cfg_t* config,
                                     const DesktopSize& size)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = 1000;

    config->g_w = size.Width();
    config->g_h = size.Height();
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

void VideoEncoderVPX::CreateVp8Codec()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

    // Adjust default target bit-rate to account for actual desktop size.
    config.rc_target_bitrate = screen_size_.Width() * screen_size_.Height() *
        config.rc_target_bitrate / config.g_w / config.g_h;

    SetCommonCodecParameters(&config, screen_size_);

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

    // Value of 16 will have the smallest CPU load. This turns off subpixel
    // motion search.
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

void VideoEncoderVPX::CreateVp9Codec()
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret) << "Failed to fetch default configuration";

    SetCommonCodecParameters(&config, screen_size_);

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

    ret = vpx_codec_control(codec_.get(),
                            VP9E_SET_TUNE_CONTENT,
                            VP9E_CONTENT_SCREEN);
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

void VideoEncoderVPX::SetActiveMap(const DesktopRect& rect)
{
    int left   = rect.Left() / kMacroBlockSize;
    int top    = rect.Top() / kMacroBlockSize;
    int right  = (rect.Right() - 1) / kMacroBlockSize;
    int bottom = (rect.Bottom() - 1) / kMacroBlockSize;

    uint8_t* map = active_map_.active_map + top * active_map_.cols;

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            map[x] = 1;
        }

        map += active_map_.cols;
    }
}

void VideoEncoderVPX::PrepareImageAndActiveMap(const DesktopFrame* frame,
                                               proto::VideoPacket* packet)
{
    memset(active_map_.active_map, 0, active_map_size_);

    int y_stride = image_.stride[0];
    int uv_stride = image_.stride[1];
    uint8_t* y_data = image_.planes[0];
    uint8_t* u_data = image_.planes[1];
    uint8_t* v_data = image_.planes[2];

    switch (image_.fmt)
    {
        case VPX_IMG_FMT_YV12:
        {
            for (DesktopRegion::Iterator iter(frame->UpdatedRegion());
                 !iter.IsAtEnd(); iter.Advance())
            {
                const DesktopRect& rect = iter.rect();

                int y_offset = y_stride * rect.y() + rect.x();
                int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                libyuv::ARGBToI420(frame->GetFrameDataAtPos(rect.LeftTop()),
                                   frame->Stride(),
                                   y_data + y_offset, y_stride,
                                   u_data + uv_offset, uv_stride,
                                   v_data + uv_offset, uv_stride,
                                   rect.Width(),
                                   rect.Height());

                ConvertToVideoRect(rect, packet->add_dirty_rect());
                SetActiveMap(rect);
            }
        }
        break;

        case VPX_IMG_FMT_I444:
        {
            for (DesktopRegion::Iterator iter(frame->UpdatedRegion());
                 !iter.IsAtEnd(); iter.Advance())
            {
                const DesktopRect& rect = iter.rect();

                int yuv_offset = uv_stride * rect.y() + rect.x();

                libyuv::ARGBToI444(frame->GetFrameDataAtPos(rect.LeftTop()),
                                   frame->Stride(),
                                   y_data + yuv_offset, y_stride,
                                   u_data + yuv_offset, uv_stride,
                                   v_data + yuv_offset, uv_stride,
                                   rect.Width(),
                                   rect.Height());

                ConvertToVideoRect(rect, packet->add_dirty_rect());
                SetActiveMap(rect);
            }
        }
        break;

        default:
        {
            DLOG(FATAL) << "Unsupported image format: " << image_.fmt;
        }
        break;
    }
}

std::unique_ptr<proto::VideoPacket> VideoEncoderVPX::Encode(
    const DesktopFrame* frame)
{
    DCHECK(encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP8 ||
           encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP9);

    std::unique_ptr<proto::VideoPacket> packet(CreateVideoPacket(encoding_));

    if (!screen_size_.IsEqual(frame->Size()))
    {
        screen_size_ = frame->Size();

        CreateImage();
        CreateActiveMap();

        if (encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP8)
        {
            CreateVp8Codec();
        }
        else if (encoding_ == proto::VideoEncoding::VIDEO_ENCODING_VP9)
        {
            CreateVp9Codec();
        }

        ConvertToVideoSize(screen_size_,
                           packet->mutable_format()->mutable_screen_size());
    }

    //
    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    //
    PrepareImageAndActiveMap(frame, packet.get());

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(),
                                            VP8E_SET_ACTIVEMAP,
                                            &active_map_);
    DCHECK_EQ(ret, VPX_CODEC_OK) << "Unable to apply active map";

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(), &image_, 0, 1, 0, VPX_DL_REALTIME);

    DCHECK_EQ(ret, VPX_CODEC_OK)
        << "Encoding error: " << vpx_codec_err_to_string(ret) << "\n"
        << "Details: " << vpx_codec_error(codec_.get()) << "\n"
        << vpx_codec_error_detail(codec_.get());

    // Read the encoded data.
    vpx_codec_iter_t iter = nullptr;

    while (true)
    {
        const vpx_codec_cx_pkt_t* vpx_packet =
            vpx_codec_get_cx_data(codec_.get(), &iter);

        if (vpx_packet && vpx_packet->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            packet->set_data(vpx_packet->data.frame.buf,
                             vpx_packet->data.frame.sz);
            break;
        }
    }

    return packet;
}

} // namespace aspia
