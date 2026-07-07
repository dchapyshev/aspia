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

#ifndef BASE_CODEC_VIDEO_ENCODER_H264_VA_H
#define BASE_CODEC_VIDEO_ENCODER_H264_VA_H

#include "base/codec/video_encoder.h"

#include <QSize>

#include <va/va.h>

#include <memory>
#include <vector>

// Hardware H.264 encoder built on top of VAAPI.
class VideoEncoderH264VA final : public VideoEncoder
{
public:
    // Constructs an encoder instance and picks a GPU with an H264 encode entrypoint. Returns
    // nullptr when no such GPU is available; callers must handle the null case.
    static std::unique_ptr<VideoEncoderH264VA> create();

    ~VideoEncoderH264VA() final;

    // Returns true when a VAAPI driver with an H264 encode entrypoint is available on this system.
    static bool isHardwareSupported();

    // VideoEncoder implementation.
    Result encode(const Frame* frame, proto::video::Packet* packet) final;
    void setBandwidth(qint64 bandwidth) final;

private:
    // A bitstream header (SPS/PPS/slice header) prepared for a packed header buffer. The data is
    // byte-padded while the driver needs the exact number of meaningful bits.
    struct PackedHeader
    {
        std::vector<quint8> data;
        quint32 bit_length = 0;
    };

    VideoEncoderH264VA();

    bool openDisplay();
    void closeDisplay();
    bool selectConfig();
    bool createEncoder(const QSize& size);
    void destroyEncoder();
    bool uploadFrame(const Frame* frame);
    bool convertIntoImage(const Frame* frame, const VAImage* image);
    bool renderFrame(bool is_idr);
    bool readCodedBuffer();

    PackedHeader buildSps() const;
    PackedHeader buildPps() const;
    PackedHeader buildSliceHeader(bool is_idr) const;

    quint32 maxRateBps() const;
    qint32 sliceQpDelta() const;

    QSize last_size_;

    int drm_fd_ = -1;
    VADisplay display_ = nullptr;
    VAProfile profile_ = VAProfileNone;
    VAEntrypoint entrypoint_ = VAEntrypointEncSlice;
    quint32 rc_mode_ = 0; // Chosen VA_RC_* rate control mode.
    quint32 packed_headers_ = 0; // VA_ENC_PACKED_HEADER_* bits the driver expects from the app.

    VAConfigID config_ = VA_INVALID_ID;
    VAContextID context_ = VA_INVALID_ID;
    VASurfaceID input_surface_ = VA_INVALID_SURFACE;
    VASurfaceID recon_surfaces_[2] = { VA_INVALID_SURFACE, VA_INVALID_SURFACE };
    VABufferID coded_buffer_ = VA_INVALID_ID;

    // Frame upload state: writing straight into the surface via vaDeriveImage is preferred; when
    // the driver cannot derive an NV12 view, a separate image plus vaPutImage is used instead.
    VAImage upload_image_ = {};
    bool use_derive_ = true;
    bool first_upload_ = false;

    int mb_width_ = 0;
    int mb_height_ = 0;
    int level_idc_ = 0;
    bool cabac_ = false;

    int current_recon_ = 0;
    quint32 frame_num_ = 0;
    quint32 idr_pic_id_ = 0;

    // Bandwidth-tuned codec parameters. Defaults match a "5 Mbps, decent quality" tier and are
    // shifted by setBandwidth(); bitrate and QP changes are picked up by the running encoder with
    // the next frame.
    bool rc_dirty_ = true;
    quint32 min_quantizer_ = 16;
    quint32 max_quantizer_ = 28;
    quint32 target_bitrate_bps_ = 5 * 1000 * 1000;

    // Fixed QP used when the driver offers no bitrate-driven rate control (VA_RC_CQP mode).
    quint32 pic_init_qp_ = 26;
    quint32 cqp_qp_ = 26;

    Q_DISABLE_COPY_MOVE(VideoEncoderH264VA)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H264_VA_H
