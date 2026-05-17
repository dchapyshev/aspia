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

#ifndef BASE_CODEC_VIDEO_DECODER_H264_H
#define BASE_CODEC_VIDEO_DECODER_H264_H

#include "base/codec/d3d11_video_context.h"
#include "base/codec/video_decoder.h"

#include <QSize>

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <memory>

// Hardware-first H.264 decoder built on Media Foundation. Falls back to the
// in-box Microsoft H.264 Decoder MFT (software, synchronous) when no hardware
// decoder is available. The HW path runs zero-copy through D3D11: decoder
// produces NV12 textures, ID3D11VideoProcessor converts to ARGB on the GPU,
// readback through a staging texture lands the pixels in Frame.
class VideoDecoderH264 final : public VideoDecoder
{
public:
    VideoDecoderH264();
    ~VideoDecoderH264() final;

    // VideoDecoder implementation.
    bool decode(const proto::video::Packet& packet, Frame* frame) final;

private:
    bool createDecoder(const QSize& size);
    void destroyDecoder();
    bool activateMft();
    bool setupHardwarePath(const QSize& size);
    bool configureMediaTypes(const QSize& size);
    bool selectOutputType();
    bool beginStreaming();
    void endStreaming();
    bool refreshOutputDimensions();
    bool allocateGpuResources(const QSize& size);

    bool waitForEvent(MediaEventType expected);
    bool feedInput(const std::string& data, quint64 sample_time_100ns);
    bool readOutput(Microsoft::WRL::ComPtr<IMFSample>* out_sample);
    bool copyHardwareSampleToFrame(
        IMFSample* sample, const proto::video::Packet& packet, Frame* frame);
    bool copySoftwareSampleToFrame(
        IMFSample* sample, const proto::video::Packet& packet, Frame* frame);

    bool mf_started_ = false;
    bool is_hardware_ = false;
    bool is_async_ = false;
    bool streaming_ = false;
    bool output_provides_samples_ = false;
    QSize last_size_;
    quint64 frame_counter_ = 0;
    quint32 output_sample_size_ = 0;

    int output_width_ = 0;
    int output_height_ = 0;
    int output_stride_ = 0;

    std::unique_ptr<D3D11VideoContext> d3d_;

    Microsoft::WRL::ComPtr<IMFTransform> decoder_;
    Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_gen_;
    Microsoft::WRL::ComPtr<IMFActivate> active_mft_;
    bool prefer_hardware_ = true;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> argb_target_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> argb_staging_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> vp_enumerator_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> vp_processor_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view_;

    DWORD input_stream_id_ = 0;
    DWORD output_stream_id_ = 0;

    Q_DISABLE_COPY_MOVE(VideoDecoderH264)
};

#endif // BASE_CODEC_VIDEO_DECODER_H264_H
