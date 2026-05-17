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

#ifndef BASE_CODEC_VIDEO_ENCODER_H264_H
#define BASE_CODEC_VIDEO_ENCODER_H264_H

#include "base/codec/d3d11_video_context.h"

#include <QSize>

#include <codecapi.h>
#include <d3d11.h>
#include <icodecapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <memory>

namespace proto::video {
enum Encoding : int;
class Packet;
} // namespace proto::video

class Frame;

// Hardware H.264 encoder built on top of an asynchronous Media Foundation Transform.
// Frames are uploaded into a D3D11 ARGB texture, converted to NV12 on the GPU via
// ID3D11VideoProcessor, and handed to the encoder as DXGI-backed samples (zero-copy
// from the encoder's point of view). Public API mirrors VideoDecoder/VideoEncoder so
// the class is drop-in once an ENCODING_H264 value is added to the protocol.
class VideoEncoderH264 final
{
public:
    explicit VideoEncoderH264();
    ~VideoEncoderH264();

    static const size_t kInitialEncodeBufferSize;

    bool encode(const Frame* frame, proto::video::Packet* packet);

    void setKeyFrameRequired(bool enable) { key_frame_required_ = enable; }
    bool isKeyFrameRequired() const { return key_frame_required_; }
    void setEncodeBuffer(std::string&& buffer) { encode_buffer_ = std::move(buffer); }

    bool setMinQuantizer(quint32 min_quantizer);
    quint32 minQuantizer() const { return min_quantizer_; }
    bool setMaxQuantizer(quint32 max_quantizer);
    quint32 maxQuantizer() const { return max_quantizer_; }

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

    bool applyMinQuantizer();
    bool applyMaxQuantizer();

    QSize last_size_;
    bool key_frame_required_ = false;
    std::string encode_buffer_;

    bool mf_started_ = false;
    bool streaming_ = false;
    bool force_key_frame_next_ = false;
    bool output_provides_samples_ = false;
    quint64 frame_counter_ = 0;
    quint32 output_sample_size_ = 0;

    quint32 min_quantizer_ = 18;
    quint32 max_quantizer_ = 40;

    std::unique_ptr<D3D11VideoContext> d3d_;

    Microsoft::WRL::ComPtr<IMFTransform> encoder_;
    Microsoft::WRL::ComPtr<ICodecAPI> codec_api_;
    Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_gen_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> argb_texture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> vp_enumerator_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> vp_processor_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> vp_input_view_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view_;

    DWORD input_stream_id_ = 0;
    DWORD output_stream_id_ = 0;

    Q_DISABLE_COPY_MOVE(VideoEncoderH264)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H264_H
