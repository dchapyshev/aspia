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

#ifndef BASE_CODEC_VIDEO_ENCODER_H264_MF_H
#define BASE_CODEC_VIDEO_ENCODER_H264_MF_H

#include "base/codec/d3d11_video_context.h"
#include "base/codec/video_encoder.h"

#include <QByteArray>
#include <QSize>

#include <codecapi.h>
#include <d3d11.h>
#include <icodecapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <memory>

// Hardware H.264 encoder built on top of an asynchronous Media Foundation Transform.
// Two ARGB-to-NV12 implementations are compiled in: a libyuv-based CPU path (default,
// high-quality chroma resampling that handles subpixel-rendered text correctly) and an
// ID3D11VideoProcessor-based GPU path (faster but uses point-sampled chroma on many
// drivers, producing color fringes on small text). The choice is a compile-time switch
// in video_encoder_h264_mf.cc.
class VideoEncoderH264MF final : public VideoEncoder
{
public:
    VideoEncoderH264MF();
    ~VideoEncoderH264MF() final;

    // Returns true when a hardware H264 encoder MFT is available on this system. Cheap to call -
    // probes the MF runtime and enumerates HW encoders without actually activating any of them.
    static bool isHardwareSupported();

    // VideoEncoder implementation.
    bool encode(const Frame* frame, proto::video::Packet* packet) final;
    void setBandwidth(qint64 bandwidth) final;

private:
    bool createEncoder(const QSize& size);
    void destroyEncoder();
    bool selectHardwareMft();
    bool configureMediaTypes(const QSize& size);
    bool configureCodecApi();
    bool beginStreaming();
    bool endStreaming();

    bool allocateGpuResources(const QSize& size);
    bool uploadArgbAndConvert(const Frame* frame);
    bool buildInputSample(quint64 sample_time_100ns, Microsoft::WRL::ComPtr<IMFSample>* out);

    bool waitForEvent(MediaEventType expected);
    bool readOutput(proto::video::Packet* packet, bool* is_key_frame_out);

    QSize last_size_;

    bool mf_started_ = false;
    bool streaming_ = false;
    bool force_key_frame_next_ = false;
    bool output_provides_samples_ = false;
    quint64 frame_counter_ = 0;
    quint32 output_sample_size_ = 0;

    // Bandwidth-tuned codec parameters. Defaults match a "5 Mbps, decent quality" tier and are
    // shifted by setBandwidth() before each codec (re)creation; configureCodecApi() reads them
    // when initializing the encoder.
    quint32 min_quantizer_ = 16;
    quint32 max_quantizer_ = 28;
    quint32 target_bitrate_bps_ = 5 * 1000 * 1000;

    std::unique_ptr<D3D11VideoContext> d3d_;

    Microsoft::WRL::ComPtr<IMFTransform> encoder_;
    Microsoft::WRL::ComPtr<ICodecAPI> codec_api_;
    Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_gen_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture_;

    // libyuv path: ARGB-to-NV12 done on the CPU, result uploaded via UpdateSubresource.
    QByteArray nv12_buffer_;
    int nv12_stride_ = 0;
    int nv12_y_rows_ = 0;

    // VideoProcessor path: ARGB-to-NV12 done on the GPU via ID3D11VideoProcessor.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> argb_texture_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> vp_enumerator_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> vp_processor_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> vp_input_view_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view_;

    DWORD input_stream_id_ = 0;
    DWORD output_stream_id_ = 0;

    Q_DISABLE_COPY_MOVE(VideoEncoderH264MF)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H264_MF_H
