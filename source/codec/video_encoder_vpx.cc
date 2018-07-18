//
// PROJECT:         Aspia
// FILE:            codec/vodeo_encoder_vpx.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_vpx.h"

#include <QDebug>
#include <QThread>

#include <libyuv/convert_from_argb.h>

#include "codec/video_util.h"
#include "desktop_capture/desktop_frame.h"

namespace aspia {

namespace {

// Defines the dimension of a macro block. This is used to compute the active map for the encoder.
constexpr int kMacroBlockSize = 16;

// Magic encoder profile numbers for I444 input formats.
constexpr int kVp9I444ProfileNumber = 1;

// Magic encoder constants for adaptive quantization strategy.
constexpr int kVp9AqModeNone = 0;

void setCommonCodecParameters(vpx_codec_enc_cfg_t* config, const QSize& size)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = 1000;

    config->g_w = size.width();
    config->g_h = size.height();
    config->g_pass = VPX_RC_ONE_PASS;

    // Start emitting packets immediately.
    config->g_lag_in_frames = 0;

    // Since the transport layer is reliable, keyframes should not be necessary.
    // However, due to crbug.com/440223, decoding fails after 30,000 non-key
    // frames, so take the hit of an "unnecessary" key-frame every 10,000 frames.
    config->kf_min_dist = 10000;
    config->kf_max_dist = 10000;

    // Using 2 threads gives a great boost in performance for most systems with
    // adequate processing power. NB: Going to multiple threads on low end
    // windows systems can really hurt performance.
    // http://crbug.com/99179
    config->g_threads = (QThread::idealThreadCount() > 2) ? 2 : 1;
}

void createImage(proto::desktop::VideoEncoding encoding,
                 const QSize& size,
                 std::unique_ptr<vpx_image_t>* out_image,
                 std::unique_ptr<quint8[]>* out_image_buffer)
{
    std::unique_ptr<vpx_image_t> image = std::make_unique<vpx_image_t>();

    memset(image.get(), 0, sizeof(vpx_image_t));

    image->d_w = image->w = size.width();
    image->d_h = image->h = size.height();

    if (encoding == proto::desktop::VIDEO_ENCODING_VP8)
    {
        image->fmt = VPX_IMG_FMT_YV12;
        image->x_chroma_shift = 1;
        image->y_chroma_shift = 1;
    }
    else
    {
        Q_ASSERT(encoding == proto::desktop::VIDEO_ENCODING_VP9);

        image->fmt = VPX_IMG_FMT_I444;
        image->x_chroma_shift = 0;
        image->y_chroma_shift = 0;
    }

    // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad the Y, U and V
    // planes' strides to multiples of 16 bytes.
    const int y_stride = ((image->w - 1) & ~15) + 16;
    const int uv_unaligned_stride = y_stride >> image->x_chroma_shift;
    const int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

    // libvpx accesses the source image in macro blocks, and will over-read if the image is not
    // padded out to the next macroblock: crbug.com/119633.
    // Pad the Y, U and V planes' height out to compensate.
    // Assuming macroblocks are 16x16, aligning the planes' strides above also macroblock aligned
    // them.
    const int y_rows = ((image->h - 1) & ~(kMacroBlockSize - 1)) + kMacroBlockSize;

    const int uv_rows = y_rows >> image->y_chroma_shift;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    const int buffer_size = y_stride * y_rows + (2 * uv_stride) * uv_rows;

    std::unique_ptr<quint8[]> image_buffer = std::make_unique<quint8[]>(buffer_size);

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(image_buffer.get(), 128, buffer_size);

    // Fill in the information
    image->planes[0] = image_buffer.get();
    image->planes[1] = image->planes[0] + y_stride * y_rows;
    image->planes[2] = image->planes[1] + uv_stride * uv_rows;

    image->stride[0] = y_stride;
    image->stride[1] = image->stride[2] = uv_stride;

    *out_image = std::move(image);
    *out_image_buffer = std::move(image_buffer);
}

} // namespace

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP8()
{
    return std::unique_ptr<VideoEncoderVPX>(
        new VideoEncoderVPX(proto::desktop::VIDEO_ENCODING_VP8));
}

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP9()
{
    return std::unique_ptr<VideoEncoderVPX>(
        new VideoEncoderVPX(proto::desktop::VIDEO_ENCODING_VP9));
}

VideoEncoderVPX::VideoEncoderVPX(proto::desktop::VideoEncoding encoding)
    : encoding_(encoding)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

void VideoEncoderVPX::createActiveMap(const QSize& size)
{
    active_map_.cols = (size.width() + kMacroBlockSize - 1) / kMacroBlockSize;
    active_map_.rows = (size.height() + kMacroBlockSize - 1) / kMacroBlockSize;
    active_map_size_ = active_map_.cols * active_map_.rows;
    active_map_buffer_ = std::make_unique<quint8[]>(active_map_size_);

    memset(active_map_buffer_.get(), 0, active_map_size_);
    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVPX::createVp8Codec(const QSize& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Adjust default target bit-rate to account for actual desktop size.
    config.rc_target_bitrate = size.width() * size.height() *
        config.rc_target_bitrate / config.g_w / config.g_h;

    setCommonCodecParameters(&config, size);

    // Value of 2 means using the real time profile. This is basically a redundant option since we
    // explicitly select real time mode when doing encoding.
    config.g_profile = 2;

    // Clamping the quantizer constrains the worst-case quality and CPU usage.
    config.rc_min_quantizer = 20;
    config.rc_max_quantizer = 30;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Value of 16 will have the smallest CPU load. This turns off subpixel motion search.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16);
    Q_ASSERT(VPX_CODEC_OK == ret);

    ret = vpx_codec_control(codec_.get(), VP8E_SET_SCREEN_CONTENT_MODE, 1);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);
}

void VideoEncoderVPX::createVp9Codec(const QSize& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);

    setCommonCodecParameters(&config, size);

    // Configure VP9 for I444 source frames.
    config.g_profile = kVp9I444ProfileNumber;

    // Disable quantization entirely, putting the encoder in "lossless" mode.
    config.rc_min_quantizer = 0;
    config.rc_max_quantizer = 0;
    config.rc_end_usage = VPX_VBR;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Request the lowest-CPU usage that VP9 supports, which depends on whether we are encoding
    // lossy or lossless.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 5);
    Q_ASSERT(VPX_CODEC_OK == ret);

    ret = vpx_codec_control(codec_.get(),
                            VP9E_SET_TUNE_CONTENT,
                            VP9E_CONTENT_SCREEN);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    Q_ASSERT(VPX_CODEC_OK == ret);

    // Set cyclic refresh (aka "top-off") only for lossy encoding.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_AQ_MODE, kVp9AqModeNone);
    Q_ASSERT(VPX_CODEC_OK == ret);
}

void VideoEncoderVPX::setActiveMap(const QRect& rect)
{
    int left   = rect.left() / kMacroBlockSize;
    int top    = rect.top() / kMacroBlockSize;
    int right  = (rect.right() - 1) / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;

    quint8* map = active_map_.active_map + top * active_map_.cols;

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            map[x] = 1;
        }

        map += active_map_.cols;
    }
}

void VideoEncoderVPX::prepareImageAndActiveMap(const DesktopFrame* frame,
                                               proto::desktop::VideoPacket* packet)
{
    memset(active_map_.active_map, 0, active_map_size_);

    int y_stride = image_->stride[0];
    int uv_stride = image_->stride[1];
    quint8* y_data = image_->planes[0];
    quint8* u_data = image_->planes[1];
    quint8* v_data = image_->planes[2];

    switch (image_->fmt)
    {
        case VPX_IMG_FMT_YV12:
        {
            for (const auto& rect : frame->constUpdatedRegion())
            {
                int y_offset = y_stride * rect.y() + rect.x();
                int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

                libyuv::ARGBToI420(frame->frameDataAtPos(rect.topLeft()),
                                   frame->stride(),
                                   y_data + y_offset, y_stride,
                                   u_data + uv_offset, uv_stride,
                                   v_data + uv_offset, uv_stride,
                                   rect.width(),
                                   rect.height());

                VideoUtil::toVideoRect(rect, packet->add_dirty_rect());
                setActiveMap(rect);
            }
        }
        break;

        case VPX_IMG_FMT_I444:
        {
            for (const auto& rect : frame->constUpdatedRegion())
            {
                int yuv_offset = uv_stride * rect.y() + rect.x();

                libyuv::ARGBToI444(frame->frameDataAtPos(rect.topLeft()),
                                   frame->stride(),
                                   y_data + yuv_offset, y_stride,
                                   u_data + yuv_offset, uv_stride,
                                   v_data + yuv_offset, uv_stride,
                                   rect.width(),
                                   rect.height());

                VideoUtil::toVideoRect(rect, packet->add_dirty_rect());
                setActiveMap(rect);
            }
        }
        break;

        default:
            qFatal("Unsupported image format: %d", image_->fmt);
            break;
    }
}

std::unique_ptr<proto::desktop::VideoPacket> VideoEncoderVPX::encode(const DesktopFrame* frame)
{
    std::unique_ptr<proto::desktop::VideoPacket> packet =
        createVideoPacket(encoding_, frame);

    if (packet->has_format())
    {
        const QSize& screen_size = frame->size();

        createImage(encoding_, screen_size, &image_, &image_buffer_);
        createActiveMap(screen_size);

        if (encoding_ == proto::desktop::VIDEO_ENCODING_VP8)
        {
            createVp8Codec(screen_size);
        }
        else
        {
            Q_ASSERT(encoding_ == proto::desktop::VIDEO_ENCODING_VP9);
            createVp9Codec(screen_size);
        }
    }

    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    prepareImageAndActiveMap(frame, packet.get());

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    Q_ASSERT(ret == VPX_CODEC_OK);

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(), image_.get(), 0, 1, 0, VPX_DL_REALTIME);
    Q_ASSERT(ret == VPX_CODEC_OK);

    // Read the encoded data.
    vpx_codec_iter_t iter = nullptr;

    while (true)
    {
        const vpx_codec_cx_pkt_t* vpx_packet = vpx_codec_get_cx_data(codec_.get(), &iter);

        if (vpx_packet && vpx_packet->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            packet->set_data(vpx_packet->data.frame.buf, vpx_packet->data.frame.sz);
            break;
        }
    }

    return packet;
}

} // namespace aspia
