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

#include "base/codec/video_encoder_vpx.h"

#include "base/logging.h"
#include "base/desktop/frame.h"

#include <libyuv/convert.h>
#include <libyuv/cpu_id.h>

#include <thread>

namespace base {

namespace {

const std::chrono::milliseconds kTargetFrameInterval{ 80 };

// Target quantizer at which stop the encoding top-off.
const int kTargetQuantizerForVp8TopOff = 30;

// Estimated size (in bytes per megapixel) of encoded frame at target quantizer value
// (see kTargetQuantizerForVp8TopOff). Compression ratio varies depending on the image, so this
// is just a rough estimate. It's used to predict when encoded "big" frame may be too large to
// be delivered to the client quickly.
static const int64_t kEstimatedBytesPerMegapixel = 100000;
static const int64_t kPixelsPerMegapixel = 1000000;

// Threshold in number of updated pixels used to detect "big" frames. These frames update
// significant portion of the screen compared to the preceding frames. For these frames min
// quantizer may need to be adjusted in order to ensure that they get delivered to the client
// as soon as possible, in exchange for lower-quality image.
static const int64_t kBigFrameThresholdPixels = 300000;

// Number of samples used to estimate processing time for the next frame.
const int kStatsWindow = 5;

// Defines the dimension of a macro block. This is used to compute the active map for the encoder.
const int kMacroBlockSize = 16;

// Magic encoder profile numbers for I420 input formats.
const int kVp9I420ProfileNumber = 0;

// Magic encoder constant for adaptive quantization strategy.
const int kVp9AqModeCyclicRefresh = 3;

const int kDefaultTargetBitrateKbps = 1000;

// Minimum target bitrate per megapixel. The value is chosen experimentally such that when screen
// is not changing the codec converges to the target quantizer above in less than 10 frames.
// This value is for VP8 only; reconsider the value for VP9.
const int kVp8MinimumTargetBitrateKbpsPerMegapixel = 2500;

void setCommonCodecParameters(vpx_codec_enc_cfg_t* config, const Size& size)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = static_cast<int>(
        std::chrono::microseconds(std::chrono::seconds(1)).count());

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
    config->g_threads = (std::thread::hardware_concurrency() + 1) / 2;

    // Do not drop any frames at encoder.
    config->rc_dropframe_thresh = 0;

    // We do not want variations in bandwidth.
    config->rc_end_usage = VPX_CBR;
    config->rc_undershoot_pct = 100;
    config->rc_overshoot_pct = 15;
}

void createImage(const Size& size,
                 std::unique_ptr<vpx_image_t>* out_image,
                 ByteArray* out_image_buffer)
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

    ByteArray image_buffer;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    image_buffer.resize(y_stride * y_rows + (2 * uv_stride) * uv_rows);

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(image_buffer.data(), 128, image_buffer.size());

    // Fill in the information.
    image->planes[0] = image_buffer.data();
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

Rect alignRect(const Rect& rect)
{
    int x = roundToTwosMultiple(rect.left());
    int y = roundToTwosMultiple(rect.top());
    int right = roundToTwosMultiple(rect.right() + 1);
    int bottom = roundToTwosMultiple(rect.bottom() + 1);

    return Rect::makeLTRB(x, y, right, bottom);
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
    : VideoEncoder(encoding),
      bitrate_filter_(kVp8MinimumTargetBitrateKbpsPerMegapixel),
      updated_region_area_(kStatsWindow)
{
    memset(&config_, 0, sizeof(config_));
    memset(&active_map_, 0, sizeof(active_map_));
}

void VideoEncoderVPX::encode(const Frame* frame, proto::VideoPacket* packet)
{
    fillPacketInfo(frame, packet);

    bool is_key_frame = false;

    if (packet->has_format())
    {
        const Size& frame_size = frame->size();

        bitrate_filter_.setFrameSize(frame_size.width(), frame_size.height());

        createImage(frame_size, &image_, &image_buffer_);
        createActiveMap(frame_size);

        if (encoding() == proto::VIDEO_ENCODING_VP8)
        {
            createVp8Codec(frame_size);
        }
        else
        {
            DCHECK_EQ(encoding(), proto::VIDEO_ENCODING_VP9);
            createVp9Codec(frame_size);
        }

        is_key_frame = true;
    }

    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    int64_t updated_area = prepareImageAndActiveMap(is_key_frame, frame, packet);

    updateConfig(updated_area);

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    DCHECK_EQ(ret, VPX_CODEC_OK);

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(),
                           image_.get(),
                           0, // pts
                           static_cast<unsigned long>(
                               std::chrono::microseconds(kTargetFrameInterval).count()),
                           0, // flags
                           VPX_DL_REALTIME);
    DCHECK_EQ(ret, VPX_CODEC_OK);

    top_off_is_active_ = config_.rc_min_quantizer > kTargetQuantizerForVp8TopOff;

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

void VideoEncoderVPX::setBandwidthEstimateKbps(int bandwidth_kbps)
{
    bitrate_filter_.setBandwidthEstimateKbps(bandwidth_kbps);
}

void VideoEncoderVPX::createActiveMap(const Size& size)
{
    active_map_.cols = (size.width() + kMacroBlockSize - 1) / kMacroBlockSize;
    active_map_.rows = (size.height() + kMacroBlockSize - 1) / kMacroBlockSize;

    active_map_buffer_.resize(active_map_.cols * active_map_.rows);
    active_map_.active_map = active_map_buffer_.data();

    clearActiveMap();
}

void VideoEncoderVPX::createVp8Codec(const Size& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config_, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    setCommonCodecParameters(&config_, size);

    // Value of 2 means using the real time profile. This is basically a redundant option since we
    // explicitly select real time mode when doing encoding.
    config_.g_profile = 2;

    // To enable remoting to be highly interactive and allow the target bitrate to be met, we relax
    // the max quantizer. The quality will get topped-off in subsequent frames.
    config_.rc_min_quantizer = 20;
    config_.rc_max_quantizer = 30;

    // In the absence of a good bandwidth estimator set the target bitrate to a
    // conservative default.
    config_.rc_target_bitrate = kDefaultTargetBitrateKbps;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config_, 0);
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

void VideoEncoderVPX::createVp9Codec(const Size& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config_, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    setCommonCodecParameters(&config_, size);

    // Configure VP9 for I420 source frames.
    config_.g_profile = kVp9I420ProfileNumber;
    config_.rc_min_quantizer = 20;
    config_.rc_max_quantizer = 30;
    config_.rc_end_usage = VPX_CBR;

    // In the absence of a good bandwidth estimator set the target bitrate to a
    // conservative default.
    config_.rc_target_bitrate = kDefaultTargetBitrateKbps;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config_, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Request the lowest-CPU usage that VP9 supports, which depends on whether we are encoding
    // lossy or lossless.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 6);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    ret = vpx_codec_control(codec_.get(), VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_NOISE_SENSITIVITY, 0);
    DCHECK_EQ(VPX_CODEC_OK, ret);

    // Set cyclic refresh (aka "top-off") only for lossy encoding.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_AQ_MODE, kVp9AqModeCyclicRefresh);
    DCHECK_EQ(VPX_CODEC_OK, ret);
}

int64_t VideoEncoderVPX::prepareImageAndActiveMap(
    bool is_key_frame, const Frame* frame, proto::VideoPacket* packet)
{
    Rect image_rect = Rect::makeWH(image_->w, image_->h);
    Region updated_region;

    if (!is_key_frame)
    {
        const int padding = ((encoding() == proto::VIDEO_ENCODING_VP9) ? 8 : 3);

        for (Region::Iterator it(frame->constUpdatedRegion()); !it.isAtEnd(); it.advance())
        {
            Rect rect = it.rect();

            // Pad each rectangle to avoid the block-artefact filters in libvpx from introducing
            // artefacts; VP9 includes up to 8px either side, and VP8 up to 3px, so unchanged
            // pixels up to that far out may still be affected by the changes in the updated
            // region, and so must be listed in the active map. After padding we align each
            // rectangle to 16x16 active-map macroblocks. This implicitly ensures all rects have
            // even top-left coords, which is is required by ARGBToI420().
            updated_region.addRect(
                alignRect(Rect::makeLTRB(
                    rect.left() - padding, rect.top() - padding,
                    rect.right() + padding, rect.bottom() + padding)));
        }

        // Clip back to the screen dimensions, in case they're not macroblock aligned.
        // The conversion routines don't require even width & height, so this is safe even if the
        // source dimensions are not even.
        updated_region.intersectWith(image_rect);
    }
    else
    {
        updated_region = Region(image_rect);
    }

    if (!top_off_is_active_)
        clearActiveMap();

    const int y_stride = image_->stride[0];
    const int uv_stride = image_->stride[1];
    uint8_t* y_data = image_->planes[0];
    uint8_t* u_data = image_->planes[1];
    uint8_t* v_data = image_->planes[2];

    int64_t updated_area = 0;

    for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
    {
        Rect rect = it.rect();

        const int y_offset = y_stride * rect.y() + rect.x();
        const int uv_offset = uv_stride * rect.y() / 2 + rect.x() / 2;
        const int width = rect.width();
        const int height = rect.height();

        libyuv::ARGBToI420(frame->frameDataAtPos(rect.topLeft()),
                           frame->stride(),
                           y_data + y_offset, y_stride,
                           u_data + uv_offset, uv_stride,
                           v_data + uv_offset, uv_stride,
                           width,
                           height);

        updated_area += width * height;
        addRectToActiveMap(rect);
    }

    if (top_off_is_active_)
        regionFromActiveMap(&updated_region);

    for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
    {
        proto::Rect* dirty_rect = packet->add_dirty_rect();
        Rect rect = it.rect();

        dirty_rect->set_x(rect.x());
        dirty_rect->set_y(rect.y());
        dirty_rect->set_width(rect.width());
        dirty_rect->set_height(rect.height());
    }

    return updated_area;
}

void VideoEncoderVPX::regionFromActiveMap(Region* updated_region)
{
    const uint8_t* map = active_map_.active_map;

    for (int y = 0; y < static_cast<int>(active_map_.rows); ++y)
    {
        for (int x0 = 0; x0 < static_cast<int>(active_map_.cols);)
        {
            int x1 = x0;

            for (; x1 < static_cast<int>(active_map_.cols); ++x1)
            {
                if (map[y * active_map_.cols + x1] == 0)
                    break;
            }

            if (x1 > x0)
            {
                updated_region->addRect(Rect::makeLTRB(
                    kMacroBlockSize * x0, kMacroBlockSize * y, kMacroBlockSize * x1,
                    kMacroBlockSize * (y + 1)));
            }

            x0 = x1 + 1;
        }
    }

    updated_region->intersectWith(Rect::makeWH(image_->w, image_->h));
}

void VideoEncoderVPX::addRectToActiveMap(const Rect& rect)
{
    int left = rect.left() / kMacroBlockSize;
    int top = rect.top() / kMacroBlockSize;
    int right = (rect.right() - 1) / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;

    uint8_t* map = active_map_.active_map + top * active_map_.cols;

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
            map[x] = 1;

        map += active_map_.cols;
    }
}

void VideoEncoderVPX::clearActiveMap()
{
    memset(active_map_buffer_.data(), 0, active_map_buffer_.size());
}

void VideoEncoderVPX::updateConfig(int64_t updated_area)
{
    bool changed = false;

    uint32_t target_bitrate = static_cast<uint32_t>(bitrate_filter_.targetBitrateKbps());
    if (config_.rc_target_bitrate != target_bitrate)
    {
        config_.rc_target_bitrate = target_bitrate;
        changed = true;
    }

    uint32_t min_quantizer = 20;
    uint32_t max_quantizer = 30;

    if (updated_area - updated_region_area_.max() > kBigFrameThresholdPixels)
    {
        int64_t expected_frame_size =
            updated_area * kEstimatedBytesPerMegapixel / kPixelsPerMegapixel;
        std::chrono::milliseconds expected_send_delay(expected_frame_size / target_bitrate);

        if (expected_send_delay > kTargetFrameInterval)
        {
            max_quantizer = 50;
            min_quantizer = 50;
        }
    }

    updated_region_area_.record(updated_area);

    if (config_.rc_min_quantizer != min_quantizer)
    {
        config_.rc_min_quantizer = min_quantizer;
        changed = true;
    }

    if (config_.rc_max_quantizer != max_quantizer)
    {
        config_.rc_max_quantizer = max_quantizer;
        changed = true;
    }

    if (!changed)
        return;

    // Update encoder context.
    if (vpx_codec_enc_config_set(codec_.get(), &config_))
        NOTREACHED() << "Unable to set encoder config";
}

} // namespace base
