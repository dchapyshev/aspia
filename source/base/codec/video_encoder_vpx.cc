//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

// Defines the dimension of a macro block. This is used to compute the active map for the encoder.
const int kMacroBlockSize = 16;

// Magic encoder profile numbers for I420 input formats.
const int kVp9I420ProfileNumber = 0;

// Magic encoder constant for adaptive quantization strategy.
const int kVp9AqModeCyclicRefresh = 3;

//--------------------------------------------------------------------------------------------------
void setCommonCodecParameters(vpx_codec_enc_cfg_t* config, const QSize& size)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = static_cast<int>(
        std::chrono::microseconds(std::chrono::seconds(1)).count());

    config->g_w = static_cast<unsigned int>(size.width());
    config->g_h = static_cast<unsigned int>(size.height());
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
    config->rc_end_usage = VPX_VBR;
    config->rc_undershoot_pct = 100;
    config->rc_overshoot_pct = 15;
}

//--------------------------------------------------------------------------------------------------
void createImage(const QSize& size,
                 std::unique_ptr<vpx_image_t>* out_image,
                 QByteArray* out_image_buffer)
{
    std::unique_ptr<vpx_image_t> image = std::make_unique<vpx_image_t>();

    memset(image.get(), 0, sizeof(vpx_image_t));

    image->d_w = image->w = static_cast<unsigned int>(size.width());
    image->d_h = image->h = static_cast<unsigned int>(size.height());

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

    QByteArray image_buffer;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    image_buffer.resize(static_cast<size_t>(y_stride * y_rows + (2 * uv_stride) * uv_rows));

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(image_buffer.data(), 128, image_buffer.size());

    // Fill in the information.
    image->planes[0] = reinterpret_cast<quint8*>(image_buffer.data());
    image->planes[1] = image->planes[0] + y_stride * y_rows;
    image->planes[2] = image->planes[1] + uv_stride * uv_rows;

    image->stride[0] = y_stride;
    image->stride[1] = image->stride[2] = uv_stride;

    *out_image = std::move(image);
    *out_image_buffer = std::move(image_buffer);
}

//--------------------------------------------------------------------------------------------------
int roundToTwosMultiple(int x)
{
    return x & (~1);
}

//--------------------------------------------------------------------------------------------------
QRect alignRect(const QRect& rect)
{
    int x = roundToTwosMultiple(rect.left());
    int y = roundToTwosMultiple(rect.top());
    int right = roundToTwosMultiple(rect.right() + 1);
    int bottom = roundToTwosMultiple(rect.bottom() + 1);

    return QRect(QPoint(x, y), QPoint(right + 1, bottom + 1));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP8()
{
    return std::unique_ptr<VideoEncoderVPX>(new VideoEncoderVPX(proto::desktop::VIDEO_ENCODING_VP8));
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoderVPX> VideoEncoderVPX::createVP9()
{
    return std::unique_ptr<VideoEncoderVPX>(new VideoEncoderVPX(proto::desktop::VIDEO_ENCODING_VP9));
}

//--------------------------------------------------------------------------------------------------
VideoEncoderVPX::VideoEncoderVPX(proto::desktop::VideoEncoding encoding)
    : VideoEncoder(encoding)
{
    memset(&config_, 0, sizeof(config_));
    memset(&active_map_, 0, sizeof(active_map_));
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderVPX::encode(const Frame* frame, proto::desktop::VideoPacket* packet)
{
    fillPacketInfo(frame, packet);

    bool is_key_frame = isKeyFrameRequired();

    if (packet->has_format())
    {
        const QSize& frame_size = frame->size();

        createImage(frame_size, &image_, &image_buffer_);
        createActiveMap(frame_size);

        if (encoding() == proto::desktop::VIDEO_ENCODING_VP8)
        {
            if (!createVp8Codec(frame_size))
            {
                LOG(ERROR) << "Unable to create VP8 codec";
                return false;
            }
        }
        else
        {
            DCHECK_EQ(encoding(), proto::desktop::VIDEO_ENCODING_VP9);

            if (!createVp9Codec(frame_size))
            {
                LOG(ERROR) << "Unable to create VP9 codec";
                return false;
            }
        }

        is_key_frame = true;
    }

    // Convert the updated capture data ready for encode.
    // Update active map based on updated region.
    prepareImageAndActiveMap(is_key_frame, frame, packet);

    // Apply active map to the encoder.
    vpx_codec_err_t ret = vpx_codec_control(codec_.get(), VP8E_SET_ACTIVEMAP, &active_map_);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_ACTIVEMAP) failed:" << ret;
        return false;
    }

    std::string* encode_buffer = encodeBuffer();
    if (encode_buffer->capacity())
    {
        encode_buffer->resize(encode_buffer->capacity());

        vpx_fixed_buf_t buffer;
        buffer.buf = encode_buffer->data();
        buffer.sz = encode_buffer->size();

        ret = vpx_codec_set_cx_data_buf(codec_.get(), &buffer, 0, 0);
        if (ret != VPX_CODEC_OK)
        {
            LOG(ERROR) << "vpx_codec_set_cx_data_buf failed:" << ret;
            return false;
        }
    }

    // Do the actual encoding.
    ret = vpx_codec_encode(codec_.get(),
                           image_.get(),
                           0, // pts
                           static_cast<unsigned long>(
                               std::chrono::microseconds(kTargetFrameInterval).count()),
                           0, // flags
                           VPX_DL_REALTIME);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_encode failed:" << ret;
        return false;
    }

    // Read the encoded data.
    vpx_codec_iter_t iter = nullptr;

    while (true)
    {
        const vpx_codec_cx_pkt_t* pkt = vpx_codec_get_cx_data(codec_.get(), &iter);
        if (!pkt)
            break;

        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            size_t frame_size = pkt->data.frame.sz;

            if (encode_buffer->capacity() < frame_size)
                encode_buffer->reserve(frame_size);

            encode_buffer->resize(frame_size);

            if (encode_buffer->data() != pkt->data.frame.buf)
                memcpy(encode_buffer->data(), pkt->data.frame.buf, frame_size);

            packet->set_data(std::move(*encode_buffer));
            break;
        }
    }

    setKeyFrameRequired(false);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderVPX::setMinQuantizer(quint32 min_quantizer)
{
    if (min_quantizer < 10 || min_quantizer > 50)
    {
        LOG(ERROR) << "Invalid quantizer value:" << min_quantizer;
        return false;
    }

    if (config_.rc_min_quantizer == min_quantizer)
    {
        LOG(INFO) << "Quantizer value not changed";
        return true;
    }

    config_.rc_min_quantizer = min_quantizer;

    vpx_codec_err_t ret = vpx_codec_enc_config_set(codec_.get(), &config_);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_set failed:" << ret;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
quint32 VideoEncoderVPX::minQuantizer() const
{
    return config_.rc_min_quantizer;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderVPX::setMaxQuantizer(quint32 max_quantizer)
{
    if (max_quantizer < 10 || max_quantizer > 60)
    {
        LOG(ERROR) << "Invalid quantizer value:" << max_quantizer;
        return false;
    }

    if (config_.rc_max_quantizer == max_quantizer)
    {
        LOG(INFO) << "Quantizer value not changed";
        return true;
    }

    config_.rc_max_quantizer = max_quantizer;

    vpx_codec_err_t ret = vpx_codec_enc_config_set(codec_.get(), &config_);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_set failed:" << ret;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
quint32 VideoEncoderVPX::maxQuantizer() const
{
    return config_.rc_max_quantizer;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderVPX::createActiveMap(const QSize& size)
{
    active_map_.cols = static_cast<unsigned int>(
        (size.width() + kMacroBlockSize - 1) / kMacroBlockSize);
    active_map_.rows = static_cast<unsigned int>(
        (size.height() + kMacroBlockSize - 1) / kMacroBlockSize);

    active_map_buffer_.resize(active_map_.cols * active_map_.rows);
    active_map_.active_map = reinterpret_cast<quint8*>(active_map_buffer_.data());

    clearActiveMap();
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderVPX::createVp8Codec(const QSize& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config_, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_default failed:" << ret;
        return false;
    }

    setCommonCodecParameters(&config_, size);

    // Value of 2 means using the real time profile. This is basically a redundant option since we
    // explicitly select real time mode when doing encoding.
    config_.g_profile = 2;

    // To enable remoting to be highly interactive and allow the target bitrate to be met, we relax
    // the max quantizer. The quality will get topped-off in subsequent frames.
    config_.rc_min_quantizer = 10;
    config_.rc_max_quantizer = 30;

    // In the absence of a good bandwidth estimator set the target bitrate to a
    // conservative default.
    config_.rc_target_bitrate = 1000;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config_, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_init failed:" << ret;
        return false;
    }

    // Value of 16 will have the smallest CPU load. This turns off subpixel motion search.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 16);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_CPUUSED) failed:" << ret;
        return false;
    }

    ret = vpx_codec_control(codec_.get(), VP8E_SET_SCREEN_CONTENT_MODE, 1);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_SCREEN_CONTENT_MODE) failed:" << ret;
        return false;
    }

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_NOISE_SENSITIVITY, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_NOISE_SENSITIVITY) failed:" << ret;
        return false;
    }

    ret = vpx_codec_control(codec_.get(), VP8E_SET_TOKEN_PARTITIONS, 3);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_TOKEN_PARTITIONS) failed:" << ret;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderVPX::createVp9Codec(const QSize& size)
{
    codec_.reset(new vpx_codec_ctx_t());

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp9_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config_, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_default failed:" << ret;
        return false;
    }

    setCommonCodecParameters(&config_, size);

    // Configure VP9 for I420 source frames.
    config_.g_profile = kVp9I420ProfileNumber;
    config_.rc_min_quantizer = 10;
    config_.rc_max_quantizer = 30;

    // In the absence of a good bandwidth estimator set the target bitrate to a
    // conservative default.
    config_.rc_target_bitrate = 1000;

    ret = vpx_codec_enc_init(codec_.get(), algo, &config_, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_init failed:" << ret;
        return false;
    }

    // Request the lowest-CPU usage that VP9 supports, which depends on whether we are encoding
    // lossy or lossless.
    ret = vpx_codec_control(codec_.get(), VP8E_SET_CPUUSED, 6);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_CPUUSED) failed:" << ret;
        return false;
    }

    ret = vpx_codec_control(codec_.get(), VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP9E_SET_TUNE_CONTENT) failed:" << ret;
        return false;
    }

    // Use the lowest level of noise sensitivity so as to spend less time on motion estimation and
    // inter-prediction mode.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_NOISE_SENSITIVITY, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP9E_SET_NOISE_SENSITIVITY) failed:" << ret;
        return false;
    }

    // Set cyclic refresh (aka "top-off") only for lossy encoding.
    ret = vpx_codec_control(codec_.get(), VP9E_SET_AQ_MODE, kVp9AqModeCyclicRefresh);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_control(VP9E_SET_AQ_MODE) failed:" << ret;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderVPX::prepareImageAndActiveMap(
    bool is_key_frame, const Frame* frame, proto::desktop::VideoPacket* packet)
{
    QRect image_rect(QPoint(0, 0), QSize(static_cast<int>(image_->w), static_cast<int>(image_->h)));
    QRegion updated_region;

    if (!is_key_frame)
    {
        const int padding = ((encoding() == proto::desktop::VIDEO_ENCODING_VP9) ? 8 : 3);

        for (const auto& rect : frame->constUpdatedRegion())
        {
            // Pad each rectangle to avoid the block-artefact filters in libvpx from introducing
            // artefacts; VP9 includes up to 8px either side, and VP8 up to 3px, so unchanged
            // pixels up to that far out may still be affected by the changes in the updated
            // region, and so must be listed in the active map. After padding we align each
            // rectangle to 16x16 active-map macroblocks. This implicitly ensures all rects have
            // even top-left coords, which is is required by ARGBToI420().
            QRect rect_with_padding =
                QRect(QPoint(rect.left() - padding, rect.top() - padding),
                      QPoint(rect.right() + padding, rect.bottom() + padding));

            updated_region += alignRect(rect_with_padding);
        }

        // Clip back to the screen dimensions, in case they're not macroblock aligned.
        // The conversion routines don't require even width & height, so this is safe even if the
        // source dimensions are not even.
        updated_region = updated_region.intersected(image_rect);
    }
    else
    {
        updated_region = image_rect;
    }

    clearActiveMap();

    const int y_stride = image_->stride[0];
    const int uv_stride = image_->stride[1];
    quint8* y_data = image_->planes[0];
    quint8* u_data = image_->planes[1];
    quint8* v_data = image_->planes[2];

    for (const auto& rect : updated_region)
    {
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

        addRectToActiveMap(rect);

        proto::desktop::Rect* dirty_rect = packet->add_dirty_rect();
        dirty_rect->set_x(rect.x());
        dirty_rect->set_y(rect.y());
        dirty_rect->set_width(width);
        dirty_rect->set_height(height);
    }
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderVPX::addRectToActiveMap(const QRect& rect)
{
    int left = rect.left() / kMacroBlockSize;
    int top = rect.top() / kMacroBlockSize;
    int right = (rect.right() - 1) / kMacroBlockSize;
    int bottom = (rect.bottom() - 1) / kMacroBlockSize;

    quint8* map = active_map_.active_map + static_cast<quint32>(top) * active_map_.cols;

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
            map[x] = 1;

        map += active_map_.cols;
    }
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderVPX::clearActiveMap()
{
    memset(active_map_buffer_.data(), 0, active_map_buffer_.size());
}

} // namespace base
