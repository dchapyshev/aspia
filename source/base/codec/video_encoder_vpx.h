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

#ifndef BASE__CODEC__VIDEO_ENCODER_VPX_H
#define BASE__CODEC__VIDEO_ENCODER_VPX_H

#include "base/macros_magic.h"
#include "base/codec/encoder_bitrate_filter.h"
#include "base/codec/running_samples.h"
#include "base/codec/scoped_vpx_codec.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/region.h"
#include "base/memory/byte_array.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

namespace base {

class VideoEncoderVPX : public VideoEncoder
{
public:
    ~VideoEncoderVPX() = default;

    static std::unique_ptr<VideoEncoderVPX> createVP8();
    static std::unique_ptr<VideoEncoderVPX> createVP9();

    void encode(const Frame* frame, proto::VideoPacket* packet) override;
    void setBandwidthEstimateKbps(int bandwidth_kbps);

private:
    explicit VideoEncoderVPX(proto::VideoEncoding encoding);

    void createActiveMap(const Size& size);
    void createVp8Codec(const Size& size);
    void createVp9Codec(const Size& size);
    int64_t prepareImageAndActiveMap(
        bool is_key_frame, const Frame* frame, proto::VideoPacket* packet);
    void regionFromActiveMap(Region* updated_region);
    void addRectToActiveMap(const Rect& rect);
    void clearActiveMap();

    void updateConfig(int64_t updated_area);

    vpx_codec_enc_cfg_t config_;
    ScopedVpxCodec codec_;

    bool top_off_is_active_ = false;
    ByteArray active_map_buffer_;
    vpx_active_map_t active_map_;

    // VPX image and buffer to hold the actual YUV planes.
    std::unique_ptr<vpx_image_t> image_;
    ByteArray image_buffer_;

    EncoderBitrateFilter bitrate_filter_;

    // Accumulator for updated region area in the previously encoded frames.
    RunningSamples updated_region_area_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderVPX);
};

} // namespace base

#endif // BASE__CODEC___VIDEO_ENCODER_VPX_H
