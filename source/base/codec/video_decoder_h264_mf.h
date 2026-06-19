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

#ifndef BASE_CODEC_VIDEO_DECODER_H264_MF_H
#define BASE_CODEC_VIDEO_DECODER_H264_MF_H

#include "base/codec/d3d11_video_context.h"
#include "base/codec/video_decoder.h"

#include <QSize>

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <memory>

// Hardware H.264 decoder built on a Media Foundation async MFT. Decode runs on the GPU through
// D3D11; the decoder produces NV12 textures which are read back into a CPU staging texture exposed
// as the NV12 frame(). Returns nullptr from create() when no HW MFT is available - callers should
// fall back to a software decoder.
class VideoDecoderH264MF final : public VideoDecoder
{
public:
    // Returns nullptr when MF runtime or HW H264 decoder MFT is not available on this system.
    static std::unique_ptr<VideoDecoderH264MF> create();

    // Cheap probe - enumerates HW decoder MFTs without activating any.
    static bool isHardwareSupported();

    ~VideoDecoderH264MF() final;

    // VideoDecoder implementation.
    Result decode(const proto::video::Packet& packet) final;
    bool isHardwareAccelerated() const final { return true; }

private:
    VideoDecoderH264MF();
    bool initialize();

    bool createDecoder(const QSize& size);
    void destroyDecoder();
    bool activateMft();
    bool configureMediaTypes(const QSize& size);
    bool selectOutputType();
    bool beginStreaming();
    void endStreaming();
    bool validateOutputType();

    bool waitForEvent(MediaEventType expected);
    bool feedInput(const std::string& data, quint64 sample_time_100ns);
    bool readOutput(Microsoft::WRL::ComPtr<IMFSample>* out_sample);
    bool mapSampleToFrame(IMFSample* sample, const QSize& size);
    void unmapStaging();

    bool mf_started_ = false;
    bool streaming_ = false;
    bool is_async_ = false;
    bool output_provides_samples_ = false;
    QSize last_size_;
    quint64 frame_counter_ = 0;
    quint32 output_sample_size_ = 0;

    std::unique_ptr<D3D11VideoContext> d3d_;

    Microsoft::WRL::ComPtr<IMFTransform> decoder_;
    Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_gen_;

    // CPU-readable copy of the decoder's NV12 output. It is kept mapped between decode() calls so
    // frame_ can point straight into it without an extra copy; unmapStaging() releases the mapping
    // before the next readback and on teardown.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_staging_;
    bool nv12_mapped_ = false;

    DWORD input_stream_id_ = 0;
    DWORD output_stream_id_ = 0;

    Q_DISABLE_COPY_MOVE(VideoDecoderH264MF)
};

#endif // BASE_CODEC_VIDEO_DECODER_H264_MF_H
