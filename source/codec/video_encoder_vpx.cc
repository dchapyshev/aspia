//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/video_encoder_vpx.h"

#include "base/logging.h"
#include "codec/video_util.h"
#include "desktop/desktop_frame.h"

#include <libyuv/convert.h>
#include <libyuv/convert_from_argb.h>

#include <thread>

namespace codec {

namespace {

// Defines the dimension of a macro block. This is used to compute the active map for the encoder.
const int kMacroBlockSize = 16;

// Magic encoder profile numbers for I444 input formats.
const int kVp9I420ProfileNumber = 0;

// Magic encoder constant for adaptive quantization strategy.
const int kVp9AqModeCyclicRefresh = 3;

void setCommonCodecParameters(vpx_codec_enc_cfg_t* config, const desktop::Size& size)
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
    config->g_threads = (std::thread::hardware_concurrency() > 2) ? 2 : 1;
}

void createImage(const desktop::Size& size,
                 std::unique_ptr<vpx_image_t>* out_image,
                 std::unique_ptr<uint8_t[]>* out_image_buffer)
{
    std::unique_ptr<vpx_image_t> image = std::make_unique<vpx_image_t>();

    memset(image.get(), 0, sizeof(vpx_image_t));

    image->d_w = image->w = size.width();
    image->d_h = image->h = size.height();

    image->fmt = VPX_IMG_FMT_YV12;
    image->x_chroma_shift = 1;
    image->y_chroma_shift = 1;

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

    std::unique_ptr<uint8_t[]> image_buffer = std::make_unique<uint8_t[]>(buffer_size);

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

int roundToTwosMultiple(int x)
{
    return x & (~1);
}

desktop::Rect alignRect(const desktop::Rect& rect)
{
    int x = roundToTwosMultiple(rect.left());
    int y = roundToTwosMultiple(rect.top());
    int right = roundToTwosMultiple(rect.right() + 1);
    int bottom = roundToTwosMultiple(rect.bottom() + 1);

    return desktop::Rect::makeLTRB(x, y, right, bottom);
}

} // namespace

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP8()
{
    return std::unique_ptr<VideoEncoderVPX>(new VideoEncoderVPX(proto::VIDEO_ENCODING_VP8));
}

// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP9()
{
    return std::unique_ptr<VideoEncoderVPX>(new VideoEncoderVPX(proto::VIDEO_ENCODING_VP9));
}

VideoEncoderVPX::VideoEncoderVPX(proto::VideoEncoding encoding)
    : encoding_(encoding)
{
    memset(&active_map_, 0, sizeof(active_map_));
    memset(&image_, 0, sizeof(image_));
}

void VideoEncoderVPX::createActiveMap(const desktop::Size& size)
{
    active_map_.cols = (size.width() + kMacroBlockSize - 1) / kMacroBlockSize;
    active_map_.rows = (size.height() + kMacroBlockSize - 1) / kMacroBlockSize;
    active_map_size_ = active_map_.cols * active_map_.rows;
    active_map_buffer_ = std::make_unique<uint8_t[]>(active_map_size_);

    memset(active_map_buffer_.get(), 0, active_map_size_);
    active_map_.active_map = active_map_buffer_.get();
}

void VideoEncoderVPX::createVp8Codec(const desktop::Size& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

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
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Value of 16 will have the smallest CPU load. This turns off subpixel motion search.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    ret = vpx_codec_control(codec_.get(), VP8E_SET_SCREEN_CONTENT_MODE, 1);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);
}

void VideoEncoderVPX::createVp9Codec(const desktop::Size& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    vpx_codec_enc_cfg_t config = { 0 };

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    setCommonCodecParameters(&config, size);

    // Configure VP9 for I420 source frames.
    config.g_profile = kVp9I420ProfileNumber;
    config.rc_min_quantizer = 20;
    config.rc_max_quantizer = 30;
    config.rc_end_usage = VPX_CBR;

    // In the absence of a good bandwidth estimator set the target bitrate to a
    // conservative default.
    config.rc_target_bitrate = 500;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Request the lowest-CPU usage that VP9 supports, which depends on whether we are encoding
    // lossy or lossless.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 6);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    ret = vpx_codec_control(codec_.get(), VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Set cyclic refresh (aka "top-off") only for lossy encoding.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_AQ_MODE, kVp9AqModeCyclicRefresh);
    DCHECK_EQ(VPX_CODEC_OK, ret);
}

void VideoEncoderVPX::setActiveMap(const desktop::Rect& rect)
{
    int left   = rect.left() / kMacroBlockSize;
    int top    = rect.top() / kMacroBlockSize;
    int right  = (rect.right() - 1) / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;

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

void VideoEncoderVPX::prepareImageAndActiveMap(
    const desktop::Frame* frame, proto::VideoPacket* packet)
{
    const int padding = ((encoding_ == proto::VIDEO_ENCODING_VP9) ? 8 : 3);

    for (desktop::Region::Iterator it(frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
    {
        const desktop::Rect& rect = it.rect();

        // Pad each rectangle to avoid the block-artefact filters in libvpx from introducing
        // artefacts; VP9 includes up to 8px either side, and VP8 up to 3px, so unchanged pixels
        // up to that far out may still be affected by the changes in the updated region, and so
        // must be listed in the active map. After padding we align each rectangle to 16x16
        // active-map macroblocks. This implicitly ensures all rects have even top-left coords,
        // which is is required by ARGBToI420().
        updated_region_.addRect(
            alignRect(desktop::Rect::makeLTRB(rect.left() - padding, rect.top() - padding,
                                            rect.right() + padding, rect.bottom() + padding)));
    }

    // Clip back to the screen dimensions, in case they're not macroblock aligned. The conversion
    // routines don't require even width & height, so this is safe even if the source dimensions
    // are not even.
    updated_region_.intersectWith(desktop::Rect::makeWH(image_->w, image_->h));

    memset(active_map_.active_map, 0, active_map_size_);

    const int y_stride = image_->stride[0];
    const int uv_stride = image_->stride[1];
    uint8_t* y_data = image_->planes[0];
    uint8_t* u_data = image_->planes[1];
    uint8_t* v_data = image_->planes[2];

    const int bits_per_pixel = frame->format().bitsPerPixel();

    for (desktop::Region::Iterator it(updated_region_); !it.isAtEnd(); it.advance())
    {
        const desktop::Rect& rect = it.rect();

        const int y_offset = y_stride * rect.y() + rect.x();
        const int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;

        if (bits_per_pixel == 32)
        {
            libyuv::ARGBToI420(frame->frameDataAtPos(rect.topLeft()),
                               frame->stride(),
                               y_data + y_offset, y_stride,
                               u_data + uv_offset, uv_stride,
                               v_data + uv_offset, uv_stride,
                               rect.width(),
                               rect.height());
        }
        else if (bits_per_pixel == 16)
        {
            libyuv::RGB565ToI420(frame->frameDataAtPos(rect.topLeft()),
                                 frame->stride(),
                                 y_data + y_offset, y_stride,
                                 u_data + uv_offset, uv_stride,
                                 v_data + uv_offset, uv_stride,
                                 rect.width(),
                                 rect.height());
        }
        else
        {
            NOTREACHED();
        }

        serializeRect(rect, packet->add_dirty_rect());
        setActiveMap(rect);
    }
}

void VideoEncoderVPX::encode(const desktop::Frame* frame, proto::VideoPacket* packet)
{
    fillPacketInfo(encoding_, frame, packet);

    if (packet->has_format())
    {
        const desktop::Size& screen_size = frame->size();

        createImage(screen_size, &image_, &image_buffer_);
        createActiveMap(screen_size);

        if (encoding_ == proto::VIDEO_ENCODING_VP8)
        {
            createVp8Codec(screen_size);
        }
        else
        {
            DCHECK_EQ(encoding_, proto::VIDEO_ENCODING_VP9);
            createVp9Codec(screen_size);
        }

        updated_region_ = desktop::Region(desktop::Rect::makeSize(screen_size));
    }
    else
    {
        updated_region_.clear();
    }

    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    prepareImageAndActiveMap(frame, packet);

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    DCHECK_EQ(ret, VPX_CODEC_OK);

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(), image_.get(), 0, 1, 0, VPX_DL_REALTIME);
    DCHECK_EQ(ret, VPX_CODEC_OK);

    // Read the encoded data.
    vpx_codec_iter_t iter = nullptr;

    while (true)
    {
        const vpx_codec_cx_pkt_t* pkt = vpx_codec_get_cx_data(codec_.get(), &iter);
        if (!pkt)
            break;

        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            packet->set_data(pkt->data.frame.buf, pkt->data.frame.sz);
            break;
        }
    }
}

} // namespace codec
