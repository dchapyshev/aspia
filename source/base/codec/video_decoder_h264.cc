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

#include "base/codec/video_decoder_h264.h"

#include "base/codec/mf_runtime.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/frame.h"
#include "base/win/scoped_co_mem.h"
#include "proto/desktop_video.h"

#include <libyuv/convert_argb.h>

#include <mferror.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

const UINT32 kAssumedFrameRateNum = 1000;
const UINT32 kAssumedFrameRateDen = 80;
const LONGLONG kFrameDuration100ns = 800000;

//--------------------------------------------------------------------------------------------------
QString hrToString(HRESULT hr)
{
    return QString("0x") + QString::number(static_cast<quint32>(hr), 16);
}

} // namespace

//--------------------------------------------------------------------------------------------------
VideoDecoderH264::VideoDecoderH264()
{
    if (!mf::isRuntimeAvailable())
    {
        LOG(ERROR) << "Media Foundation runtime not available on this system";
        return;
    }

    HRESULT hr = mf::startup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr))
    {
        LOG(ERROR) << "MFStartup failed:" << hrToString(hr);
        return;
    }
    mf_started_ = true;
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264::~VideoDecoderH264()
{
    destroyDecoder();

    if (mf_started_)
        mf::shutdown();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::decode(const proto::video::Packet& packet, Frame* frame)
{
    if (!mf_started_ || !frame)
        return false;

    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return false;
    }

    if (!decoder_ || last_size_ != frame->size())
    {
        last_size_ = frame->size();

        destroyDecoder();
        if (!createDecoder(last_size_))
        {
            LOG(ERROR) << "Unable to create H264 decoder";
            return false;
        }
    }

    const quint64 sample_time = frame_counter_ * static_cast<quint64>(kFrameDuration100ns);

    if (is_async_)
    {
        if (!waitForEvent(METransformNeedInput))
            return false;
    }

    if (!feedInput(packet.data(), sample_time))
        return false;

    ComPtr<IMFSample> sample;

    if (is_async_)
    {
        if (!waitForEvent(METransformHaveOutput))
            return false;
        if (!readOutput(&sample))
            return false;
    }
    else
    {
        // SW MFT may buffer one frame internally; in that case readOutput returns false with
        // MF_E_TRANSFORM_NEED_MORE_INPUT and the caller will get the frame on the next packet.
        if (!readOutput(&sample))
            return false;
    }

    if (!sample)
        return false;

    const bool ok = is_hardware_ ?
        copyHardwareSampleToFrame(sample.Get(), packet, frame) :
        copySoftwareSampleToFrame(sample.Get(), packet, frame);

    if (!ok)
        return false;

    ++frame_counter_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::createDecoder(const QSize& size)
{
    // First attempt: hardware async MFT with D3D11 zero-copy. If anything in the chain fails,
    // fall back to the in-box software MFT with system-memory NV12 readback.
    prefer_hardware_ = true;
    if (activateMft())
    {
        is_hardware_ = true;
        if (setupHardwarePath(size))
            return true;

        LOG(WARNING) << "Hardware D3D11 path setup failed, falling back to software decoder";
        destroyDecoder();
    }

    prefer_hardware_ = false;
    if (!activateMft())
    {
        LOG(ERROR) << "No H264 decoder MFT available";
        return false;
    }
    is_hardware_ = false;

    HRESULT hr = decoder_->GetStreamIDs(1, &input_stream_id_, 1, &output_stream_id_);
    if (hr == E_NOTIMPL)
    {
        input_stream_id_ = 0;
        output_stream_id_ = 0;
        hr = S_OK;
    }
    if (FAILED(hr))
    {
        LOG(ERROR) << "IMFTransform::GetStreamIDs failed:" << hrToString(hr);
        return false;
    }

    ComPtr<IMFAttributes> attrs;
    if (SUCCEEDED(decoder_->GetAttributes(&attrs)) && attrs)
        attrs->SetUINT32(MF_LOW_LATENCY, TRUE);

    LOG(INFO) << "H264 decoder selected: software";

    if (!configureMediaTypes(size))
        return false;

    if (!refreshOutputDimensions())
        return false;

    MFT_OUTPUT_STREAM_INFO out_info;
    memset(&out_info, 0, sizeof(out_info));
    hr = decoder_->GetOutputStreamInfo(output_stream_id_, &out_info);
    if (FAILED(hr))
    {
        LOG(ERROR) << "GetOutputStreamInfo failed:" << hrToString(hr);
        return false;
    }
    output_provides_samples_ =
        (out_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                             MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
    output_sample_size_ = out_info.cbSize;

    if (!beginStreaming())
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264::destroyDecoder()
{
    if (streaming_)
        endStreaming();

    vp_output_view_.Reset();
    vp_processor_.Reset();
    vp_enumerator_.Reset();
    argb_target_.Reset();
    argb_staging_.Reset();

    event_gen_.Reset();
    if (decoder_)
    {
        decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0);
        decoder_.Reset();
    }
    active_mft_.Reset();

    d3d_.reset();

    is_hardware_ = false;
    is_async_ = false;
    output_provides_samples_ = false;
    output_sample_size_ = 0;
    output_width_ = 0;
    output_height_ = 0;
    output_stride_ = 0;
    frame_counter_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::activateMft()
{
    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };

    const UINT32 flags = prefer_hardware_ ?
        (MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER) :
        (MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER);

    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    HRESULT hr = mf::enumTransforms(
        MFT_CATEGORY_VIDEO_DECODER, flags, &input_info, nullptr, &activate_arr, &count);
    if (FAILED(hr) || count == 0)
        return false;

    hr = activate_arr.get()[0]->ActivateObject(IID_PPV_ARGS(&decoder_));
    if (SUCCEEDED(hr))
        active_mft_ = activate_arr.get()[0];

    for (UINT32 i = 0; i < count; ++i)
        activate_arr.get()[i]->Release();

    if (FAILED(hr))
    {
        LOG(WARNING) << "IMFActivate::ActivateObject failed:" << hrToString(hr);
        decoder_.Reset();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::setupHardwarePath(const QSize& size)
{
    d3d_ = D3D11VideoContext::create();
    if (!d3d_)
    {
        LOG(WARNING) << "Failed to create D3D11 context";
        return false;
    }

    HRESULT hr = decoder_->GetStreamIDs(1, &input_stream_id_, 1, &output_stream_id_);
    if (hr == E_NOTIMPL)
    {
        input_stream_id_ = 0;
        output_stream_id_ = 0;
        hr = S_OK;
    }
    if (FAILED(hr))
    {
        LOG(WARNING) << "IMFTransform::GetStreamIDs failed:" << hrToString(hr);
        return false;
    }

    ComPtr<IMFAttributes> attrs;
    hr = decoder_->GetAttributes(&attrs);
    if (FAILED(hr) || !attrs)
    {
        LOG(WARNING) << "GetAttributes failed:" << hrToString(hr);
        return false;
    }
    attrs->SetUINT32(MF_LOW_LATENCY, TRUE);

    UINT32 async_flag = 0;
    if (SUCCEEDED(attrs->GetUINT32(MF_TRANSFORM_ASYNC, &async_flag)) && async_flag)
    {
        is_async_ = true;
        hr = attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
        if (FAILED(hr))
        {
            LOG(WARNING) << "Failed to unlock async MFT:" << hrToString(hr);
            return false;
        }
        hr = decoder_.As(&event_gen_);
        if (FAILED(hr))
        {
            LOG(WARNING) << "MFT does not expose IMFMediaEventGenerator:" << hrToString(hr);
            return false;
        }
    }

    hr = decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(d3d_->manager()));
    if (FAILED(hr))
    {
        LOG(WARNING) << "MFT_MESSAGE_SET_D3D_MANAGER failed:" << hrToString(hr);
        return false;
    }

    LOG(INFO) << "H264 decoder selected: hardware (D3D11)";

    if (!configureMediaTypes(size))
        return false;

    if (!refreshOutputDimensions())
        return false;

    MFT_OUTPUT_STREAM_INFO out_info;
    memset(&out_info, 0, sizeof(out_info));
    hr = decoder_->GetOutputStreamInfo(output_stream_id_, &out_info);
    if (FAILED(hr))
    {
        LOG(WARNING) << "GetOutputStreamInfo failed:" << hrToString(hr);
        return false;
    }
    output_provides_samples_ =
        (out_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                             MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
    output_sample_size_ = out_info.cbSize;

    if (!allocateGpuResources(size))
        return false;

    if (!beginStreaming())
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::configureMediaTypes(const QSize& size)
{
    ComPtr<IMFMediaType> input_type;
    HRESULT hr = mf::createMediaType(&input_type);
    if (FAILED(hr))
    {
        LOG(ERROR) << "MFCreateMediaType (input) failed:" << hrToString(hr);
        return false;
    }

    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(size.width()), static_cast<UINT32>(size.height()));
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, kAssumedFrameRateNum, kAssumedFrameRateDen);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = decoder_->SetInputType(input_stream_id_, input_type.Get(), 0);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SetInputType failed:" << hrToString(hr);
        return false;
    }

    return selectOutputType();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::selectOutputType()
{
    DWORD index = 0;
    while (true)
    {
        ComPtr<IMFMediaType> candidate;
        HRESULT hr = decoder_->GetOutputAvailableType(output_stream_id_, index, &candidate);
        if (hr == MF_E_NO_MORE_TYPES)
            break;
        if (FAILED(hr))
        {
            LOG(ERROR) << "GetOutputAvailableType failed:" << hrToString(hr);
            return false;
        }
        ++index;

        GUID subtype = GUID_NULL;
        candidate->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype != MFVideoFormat_NV12)
            continue;

        hr = decoder_->SetOutputType(output_stream_id_, candidate.Get(), 0);
        if (FAILED(hr))
        {
            LOG(ERROR) << "SetOutputType failed:" << hrToString(hr);
            return false;
        }
        return true;
    }

    LOG(ERROR) << "No NV12 output type available on H264 decoder";
    return false;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::beginStreaming()
{
    decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    streaming_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264::endStreaming()
{
    if (decoder_)
    {
        decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
    streaming_ = false;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::refreshOutputDimensions()
{
    ComPtr<IMFMediaType> out_type;
    HRESULT hr = decoder_->GetOutputCurrentType(output_stream_id_, &out_type);
    if (FAILED(hr))
    {
        LOG(ERROR) << "GetOutputCurrentType failed:" << hrToString(hr);
        return false;
    }

    UINT32 width = 0;
    UINT32 height = 0;
    MFGetAttributeSize(out_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    output_width_ = static_cast<int>(width);
    output_height_ = static_cast<int>(height);

    UINT32 stride = 0;
    if (SUCCEEDED(out_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)))
        output_stride_ = static_cast<int>(stride);
    else
        output_stride_ = output_width_;

    return output_width_ > 0 && output_height_ > 0;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::allocateGpuResources(const QSize& size)
{
    argb_target_ = d3d_->createDefaultArgbTexture(size.width(), size.height());
    if (!argb_target_)
        return false;

    argb_staging_ = d3d_->createStagingArgbTexture(size.width(), size.height());
    if (!argb_staging_)
        return false;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;
    memset(&content_desc, 0, sizeof(content_desc));
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputFrameRate.Numerator = kAssumedFrameRateNum;
    content_desc.InputFrameRate.Denominator = kAssumedFrameRateDen;
    content_desc.InputWidth = static_cast<UINT>(size.width());
    content_desc.InputHeight = static_cast<UINT>(size.height());
    content_desc.OutputFrameRate.Numerator = kAssumedFrameRateNum;
    content_desc.OutputFrameRate.Denominator = kAssumedFrameRateDen;
    content_desc.OutputWidth = static_cast<UINT>(size.width());
    content_desc.OutputHeight = static_cast<UINT>(size.height());
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = d3d_->videoDevice()->CreateVideoProcessorEnumerator(&content_desc, &vp_enumerator_);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateVideoProcessorEnumerator failed:" << hrToString(hr);
        return false;
    }

    hr = d3d_->videoDevice()->CreateVideoProcessor(vp_enumerator_.Get(), 0, &vp_processor_);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateVideoProcessor failed:" << hrToString(hr);
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov_desc;
    memset(&ov_desc, 0, sizeof(ov_desc));
    ov_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ov_desc.Texture2D.MipSlice = 0;

    hr = d3d_->videoDevice()->CreateVideoProcessorOutputView(
        argb_target_.Get(), vp_enumerator_.Get(), &ov_desc, &vp_output_view_);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateVideoProcessorOutputView failed:" << hrToString(hr);
        return false;
    }

    const RECT full_rect = { 0, 0, size.width(), size.height() };
    d3d_->videoContext()->VideoProcessorSetStreamSourceRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetStreamDestRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetOutputTargetRect(vp_processor_.Get(), TRUE, &full_rect);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::waitForEvent(MediaEventType expected)
{
    while (true)
    {
        ComPtr<IMFMediaEvent> event;
        HRESULT hr = event_gen_->GetEvent(0, &event);
        if (FAILED(hr))
        {
            LOG(ERROR) << "IMFMediaEventGenerator::GetEvent failed:" << hrToString(hr);
            return false;
        }

        MediaEventType type = MEUnknown;
        event->GetType(&type);

        if (type == expected)
            return true;

        if (type == METransformDrainComplete || type == METransformMarker)
            continue;

        LOG(ERROR) << "Unexpected MFT event:" << type << "expected:" << expected;
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::feedInput(const std::string& data, quint64 sample_time_100ns)
{
    const DWORD data_size = static_cast<DWORD>(data.size());

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = mf::createAlignedMemoryBuffer(data_size, MF_16_BYTE_ALIGNMENT, &buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "MFCreateAlignedMemoryBuffer failed:" << hrToString(hr);
        return false;
    }

    BYTE* dst = nullptr;
    hr = buffer->Lock(&dst, nullptr, nullptr);
    if (FAILED(hr))
    {
        LOG(ERROR) << "IMFMediaBuffer::Lock failed:" << hrToString(hr);
        return false;
    }
    memcpy(dst, data.data(), data_size);
    buffer->Unlock();
    buffer->SetCurrentLength(data_size);

    ComPtr<IMFSample> sample;
    hr = mf::createSample(&sample);
    if (FAILED(hr))
    {
        LOG(ERROR) << "MFCreateSample failed:" << hrToString(hr);
        return false;
    }
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(sample_time_100ns));
    sample->SetSampleDuration(kFrameDuration100ns);

    hr = decoder_->ProcessInput(input_stream_id_, sample.Get(), 0);
    if (FAILED(hr))
    {
        LOG(ERROR) << "IMFTransform::ProcessInput failed:" << hrToString(hr);
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::readOutput(ComPtr<IMFSample>* out_sample)
{
    out_sample->Reset();

    MFT_OUTPUT_DATA_BUFFER out;
    memset(&out, 0, sizeof(out));
    out.dwStreamID = output_stream_id_;

    ComPtr<IMFSample> allocated_sample;
    ComPtr<IMFMediaBuffer> allocated_buffer;
    if (!output_provides_samples_)
    {
        HRESULT hr = mf::createSample(&allocated_sample);
        if (FAILED(hr))
        {
            LOG(ERROR) << "MFCreateSample (output) failed:" << hrToString(hr);
            return false;
        }
        hr = mf::createAlignedMemoryBuffer(
            std::max<DWORD>(output_sample_size_, 1u), MF_16_BYTE_ALIGNMENT, &allocated_buffer);
        if (FAILED(hr))
        {
            LOG(ERROR) << "MFCreateAlignedMemoryBuffer (output) failed:" << hrToString(hr);
            return false;
        }
        allocated_sample->AddBuffer(allocated_buffer.Get());
        out.pSample = allocated_sample.Get();
    }

    DWORD status = 0;
    HRESULT hr = decoder_->ProcessOutput(0, 1, &out, &status);

    // Take ownership of refs the MFT may have written into the struct, regardless of hr.
    ComPtr<IMFCollection> events;
    events.Attach(out.pEvents);
    out.pEvents = nullptr;

    ComPtr<IMFSample> sample;
    if (output_provides_samples_)
    {
        sample.Attach(out.pSample);
        out.pSample = nullptr;
    }
    else
    {
        sample = allocated_sample;
    }

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
    {
        if (!selectOutputType() || !refreshOutputDimensions())
            return false;
        return readOutput(out_sample);
    }

    if (FAILED(hr))
    {
        if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT)
            LOG(ERROR) << "IMFTransform::ProcessOutput failed:" << hrToString(hr);
        return false;
    }

    if (!sample)
    {
        LOG(ERROR) << "ProcessOutput returned no sample";
        return false;
    }

    *out_sample = sample;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::copyHardwareSampleToFrame(
    IMFSample* sample, const proto::video::Packet& packet, Frame* frame)
{
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->GetBufferByIndex(0, &buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "IMFSample::GetBufferByIndex failed:" << hrToString(hr);
        return false;
    }

    ComPtr<IMFDXGIBuffer> dxgi_buffer;
    hr = buffer.As(&dxgi_buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "Sample buffer is not DXGI-backed:" << hrToString(hr);
        return false;
    }

    ComPtr<ID3D11Texture2D> nv12_texture;
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&nv12_texture));
    if (FAILED(hr))
    {
        LOG(ERROR) << "IMFDXGIBuffer::GetResource failed:" << hrToString(hr);
        return false;
    }

    UINT subresource = 0;
    dxgi_buffer->GetSubresourceIndex(&subresource);

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv_desc;
    memset(&iv_desc, 0, sizeof(iv_desc));
    iv_desc.FourCC = 0;
    iv_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    iv_desc.Texture2D.MipSlice = 0;
    iv_desc.Texture2D.ArraySlice = subresource;

    ComPtr<ID3D11VideoProcessorInputView> input_view;
    hr = d3d_->videoDevice()->CreateVideoProcessorInputView(
        nv12_texture.Get(), vp_enumerator_.Get(), &iv_desc, &input_view);
    if (FAILED(hr))
    {
        LOG(ERROR) << "CreateVideoProcessorInputView failed:" << hrToString(hr);
        return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream;
    memset(&stream, 0, sizeof(stream));
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.pInputSurface = input_view.Get();

    hr = d3d_->videoContext()->VideoProcessorBlt(vp_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    if (FAILED(hr))
    {
        LOG(ERROR) << "VideoProcessorBlt failed:" << hrToString(hr);
        return false;
    }

    d3d_->deviceContext()->CopyResource(argb_staging_.Get(), argb_target_.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d3d_->deviceContext()->Map(argb_staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
        LOG(ERROR) << "Map argb_staging_ failed:" << hrToString(hr);
        return false;
    }

    QRect frame_rect(QPoint(0, 0), frame->size());
    const quint8* src_base = static_cast<const quint8*>(mapped.pData);
    bool ok = true;

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = parse(packet.dirty_rect(i));
        if (!frame_rect.contains(rect))
        {
            LOG(ERROR) << "The rectangle is outside the screen area";
            ok = false;
            break;
        }

        const int row_bytes = rect.width() * 4;
        const quint8* src = src_base + rect.y() * mapped.RowPitch + rect.x() * 4;
        quint8* dst = frame->frameDataAtPos(rect.topLeft());
        const int dst_stride = frame->stride();

        for (int y = 0; y < rect.height(); ++y)
            memcpy(dst + y * dst_stride, src + y * mapped.RowPitch, row_bytes);
    }

    d3d_->deviceContext()->Unmap(argb_staging_.Get(), 0);
    return ok;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264::copySoftwareSampleToFrame(
    IMFSample* sample, const proto::video::Packet& packet, Frame* frame)
{
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "ConvertToContiguousBuffer failed:" << hrToString(hr);
        return false;
    }

    BYTE* base = nullptr;
    LONG pitch = 0;
    ComPtr<IMF2DBuffer> buffer_2d;
    if (SUCCEEDED(buffer.As(&buffer_2d)))
    {
        hr = buffer_2d->Lock2D(&base, &pitch);
        if (FAILED(hr))
        {
            LOG(ERROR) << "IMF2DBuffer::Lock2D failed:" << hrToString(hr);
            return false;
        }
    }
    else
    {
        DWORD current_length = 0;
        hr = buffer->Lock(&base, nullptr, &current_length);
        if (FAILED(hr))
        {
            LOG(ERROR) << "IMFMediaBuffer::Lock failed:" << hrToString(hr);
            return false;
        }
        pitch = output_stride_ > 0 ? output_stride_ : output_width_;
    }

    const int y_stride = pitch;
    const int uv_stride = pitch;
    const quint8* y_plane = base;
    const quint8* uv_plane = base + y_stride * output_height_;

    QRect frame_rect(QPoint(0, 0), frame->size());
    bool ok = true;

    for (int i = 0; i < packet.dirty_rect_size(); ++i)
    {
        QRect rect = parse(packet.dirty_rect(i));
        if (!frame_rect.contains(rect))
        {
            LOG(ERROR) << "The rectangle is outside the screen area";
            ok = false;
            break;
        }

        const int y_offset = rect.y() * y_stride + rect.x();
        const int uv_offset = (rect.y() / 2) * uv_stride + (rect.x() & ~1);

        libyuv::NV12ToARGB(y_plane + y_offset, y_stride,
                           uv_plane + uv_offset, uv_stride,
                           frame->frameDataAtPos(rect.topLeft()), frame->stride(),
                           rect.width(), rect.height());
    }

    if (buffer_2d)
        buffer_2d->Unlock2D();
    else
        buffer->Unlock();

    return ok;
}
