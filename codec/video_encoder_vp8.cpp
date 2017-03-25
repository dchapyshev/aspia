//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/vodeo_encoder_vp8.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_vp8.h"

#include <libyuv/convert_from_argb.h>
#include "base/logging.h"
#include "base/cpu.h"

namespace aspia {

static const int kBlockSize = 16;

VideoEncoderVP8::VideoEncoderVP8() :
    codec_(nullptr),
    active_map_size_(0)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

void VideoEncoderVP8::CreateImage()
{
    memset(&image_, 0, sizeof(image_));

    image_.d_w = image_.w = screen_size_.Width();
    image_.d_h = image_.h = screen_size_.Height();

    image_.fmt = VPX_IMG_FMT_YV12;
    image_.x_chroma_shift = 1;
    image_.y_chroma_shift = 1;

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

    image_.stride[0] = y_stride;
    image_.stride[1] = image_.stride[2] = uv_stride;
}

void VideoEncoderVP8::CreateActiveMap()
{
    active_map_.cols = (screen_size_.Width() + kBlockSize - 1) / kBlockSize;
    active_map_.rows = (screen_size_.Height() + kBlockSize - 1) / kBlockSize;

    active_map_size_ = active_map_.cols * active_map_.rows;

    active_map_buffer_.reset(new uint8_t[active_map_size_]);

    memset(active_map_buffer_.get(), 0, active_map_size_);

    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVP8::SetCommonCodecParameters(vpx_codec_enc_cfg_t* config)
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
    config->g_threads = (GetNumberOfProcessors() > 2) ? 2 : 1;
}

void VideoEncoderVP8::CreateCodec()
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

void VideoEncoderVP8::PrepareImageAndActiveMap(const DesktopFrame* frame,
                                               proto::VideoPacket* packet)
{
    memset(active_map_.active_map, 0, active_map_size_);

    int y_stride = image_.stride[0];
    int uv_stride = image_.stride[1];
    uint8_t* y_data = image_.planes[0];
    uint8_t* u_data = image_.planes[1];
    uint8_t* v_data = image_.planes[2];

    for (DesktopRegion::Iterator iter(frame->UpdatedRegion()); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect& rect = iter.rect();

        int rgb_offset = frame->Stride() * rect.y() + rect.x() * frame->Format().BytesPerPixel();
        int y_offset = y_stride * rect.y() + rect.x();
        int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;
        int width = rect.Width();
        int height = rect.Height();

        libyuv::ARGBToI420(frame->GetFrameData() + rgb_offset, frame->Stride(),
                           y_data + y_offset, y_stride,
                           u_data + uv_offset, uv_stride,
                           v_data + uv_offset, uv_stride,
                           width,
                           height);

        rect.ToVideoRect(packet->add_dirty_rect());

        int left   = rect.Left() / kBlockSize;
        int top    = rect.Top() / kBlockSize;
        int right  = (rect.Right() - 1) / kBlockSize;
        int bottom = (rect.Bottom() - 1) / kBlockSize;

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
}

void VideoEncoderVP8::Encode(proto::VideoPacket* packet, const DesktopFrame* frame)
{
    packet->set_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);

    if (!screen_size_.IsEqual(frame->Size()))
    {
        screen_size_ = frame->Size();

        CreateImage();
        CreateActiveMap();
        CreateCodec();

        screen_size_.ToVideoSize(packet->mutable_screen_size());

        frame->InvalidateFrame();
    }

    //
    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    //
    PrepareImageAndActiveMap(frame, packet);

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

    while (true)
    {
        const vpx_codec_cx_pkt_t* vpx_packet =
            vpx_codec_get_cx_data(codec_.get(), &iter);

        if (vpx_packet && vpx_packet->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
            break;
        }
    }
}

} // namespace aspia
