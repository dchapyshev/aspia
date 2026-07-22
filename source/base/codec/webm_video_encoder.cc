//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/webm_video_encoder.h"

#include <QThread>

#include "base/logging.h"
#include "base/time_types.h"
#include "proto/desktop_video.h"

#include <libyuv/convert.h>

namespace {

// Defines the dimension of a macro block.
const int kMacroBlockSize = 16;

const Milliseconds kTargetFrameInterval{ 80 };

//--------------------------------------------------------------------------------------------------
void setCodecParameters(vpx_codec_enc_cfg_t* config, const QSize& size)
{
    // Use millisecond granularity time base.
    config->g_timebase.num = 1;
    config->g_timebase.den = static_cast<int>(Microseconds(Seconds(1)).count());

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
    config->g_threads = (QThread::idealThreadCount() + 1) / 2;

    // Do not drop any frames at encoder.
    config->rc_dropframe_thresh = 0;

    // We do not want variations in bandwidth.
    config->rc_end_usage = VPX_CBR;
    config->rc_undershoot_pct = 100;
    config->rc_overshoot_pct = 15;
}

} // namespace

//--------------------------------------------------------------------------------------------------
WebmVideoEncoder::WebmVideoEncoder()
{
    LOG(INFO) << "Ctor";
    memset(&config_, 0, sizeof(config_));
}

//--------------------------------------------------------------------------------------------------
WebmVideoEncoder::~WebmVideoEncoder()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool WebmVideoEncoder::encode(const VideoDecoder::YuvView& frame, proto::video::Packet* packet)
{
    DCHECK(packet);

    packet->set_encoding(proto::video::ENCODING_VP8);

    const bool size_changed = (last_frame_size_ != frame.size());
    if (size_changed || last_frame_format_ != frame.format())
    {
        last_frame_size_ = frame.size();
        last_frame_format_ = frame.format();
        createImage();
    }

    if (size_changed)
    {
        LOG(INFO) << "Frame size changed to" << frame.size();

        if (!createCodec())
        {
            LOG(ERROR) << "createCodec failed";
            return false;
        }

        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(last_frame_size_.width());
        video_rect->set_height(last_frame_size_.height());
    }

    if (last_frame_format_ == VideoDecoder::YuvFormat::NV12)
    {
        // VP8 needs planar I420, so split the interleaved chroma into the owned image buffer.
        libyuv::NV12ToI420(frame.planeData(0), frame.planeStride(0),
                           frame.planeData(1), frame.planeStride(1),
                           image_->planes[0], image_->stride[0],
                           image_->planes[1], image_->stride[1],
                           image_->planes[2], image_->stride[2],
                           last_frame_size_.width(),
                           last_frame_size_.height());
    }
    else
    {
        image_->planes[0] = const_cast<quint8*>(frame.planeData(0));
        image_->planes[1] = const_cast<quint8*>(frame.planeData(1));
        image_->planes[2] = const_cast<quint8*>(frame.planeData(2));
        image_->stride[0] = frame.planeStride(0);
        image_->stride[1] = frame.planeStride(1);
        image_->stride[2] = frame.planeStride(2);
    }

    // Do the actual encoding.
    vpx_codec_err_t ret = vpx_codec_encode(
        codec_.get(),
        image_.get(),
        0, // pts
        static_cast<unsigned long>(Microseconds(kTargetFrameInterval).count()),
        0, // flags
        VPX_DL_REALTIME);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_encode failed";
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
            packet->set_data(pkt->data.frame.buf, pkt->data.frame.sz);
            break;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void WebmVideoEncoder::createImage()
{
    image_ = std::make_unique<vpx_image_t>();

    memset(image_.get(), 0, sizeof(vpx_image_t));

    image_->d_w = image_->w = static_cast<unsigned int>(last_frame_size_.width());
    image_->d_h = image_->h = static_cast<unsigned int>(last_frame_size_.height());

    image_->fmt = VPX_IMG_FMT_YV12;
    image_->x_chroma_shift = 1;
    image_->y_chroma_shift = 1;

    // An I420 frame is fed to the encoder straight from the decoder's planes (set per encode), so no
    // buffer is needed here. NV12 is converted into this owned, macroblock-padded I420 buffer.
    if (last_frame_format_ != VideoDecoder::YuvFormat::NV12)
        return;

    // libyuv's fast-path requires 16-byte aligned pointers and strides, so pad the Y, U and V
    // planes' strides to multiples of 16 bytes.
    const int y_stride = ((static_cast<int>(image_->w) - 1) & ~15) + 16;
    const int uv_unaligned_stride = y_stride >> image_->x_chroma_shift;
    const int uv_stride = ((uv_unaligned_stride - 1) & ~15) + 16;

    // libvpx accesses the source image in macro blocks, and will over-read if the image is not
    // padded out to the next macroblock: crbug.com/119633.
    // Pad the Y, U and V planes' height out to compensate.
    // Assuming macroblocks are 16x16, aligning the planes' strides above also macroblock aligned
    // them.
    const int y_rows = ((static_cast<int>(image_->h) - 1) & ~(kMacroBlockSize - 1)) + kMacroBlockSize;
    const int uv_rows = y_rows >> image_->y_chroma_shift;

    // Allocate a YUV buffer large enough for the aligned data & padding.
    image_buffer_.resize(static_cast<size_t>(y_stride * y_rows + (2 * uv_stride) * uv_rows));

    // Reset image value to 128 so we just need to fill in the y plane.
    memset(image_buffer_.data(), 128, image_buffer_.size());

    // Fill in the information.
    image_->planes[0] = reinterpret_cast<quint8*>(image_buffer_.data());
    image_->planes[1] = image_->planes[0] + y_stride * y_rows;
    image_->planes[2] = image_->planes[1] + uv_stride * uv_rows;

    image_->stride[0] = y_stride;
    image_->stride[1] = image_->stride[2] = uv_stride;
}

//--------------------------------------------------------------------------------------------------
bool WebmVideoEncoder::createCodec()
{
    codec_.reset(new vpx_codec_ctx_t());

    // Configure the encoder.
    vpx_codec_iface_t* algo = vpx_codec_vp8_cx();

    vpx_codec_err_t ret = vpx_codec_enc_config_default(algo, &config_, 0);
    if (ret != VPX_CODEC_OK)
    {
        LOG(ERROR) << "vpx_codec_enc_config_default failed";
        return false;
    }

    setCodecParameters(&config_, last_frame_size_);

    // Value of 2 means using the real time profile. This is basically a redundant option since we
    // explicitly select real time mode when doing encoding.
    config_.g_profile = 2;

    // To enable remoting to be highly interactive and allow the target bitrate to be met, we relax
    // the max quantizer. The quality will get topped-off in subsequent frames.
    config_.rc_min_quantizer = 20;
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
        LOG(ERROR) << "vpx_codec_control(VP8E_SET_NOISE_SENSITIVITY) failed";
        return false;
    }

    return true;
}
