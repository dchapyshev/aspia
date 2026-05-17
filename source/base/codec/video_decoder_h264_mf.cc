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

#include "base/codec/video_decoder_h264_mf.h"

#include "base/codec/mf_runtime.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/desktop/frame.h"
#include "base/win/scoped_co_mem.h"
#include "proto/desktop_video.h"

#include <libyuv/convert_argb.h>

#include <comdef.h>
#include <mferror.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

const UINT32 kAssumedFrameRateNum = 1000;
const UINT32 kAssumedFrameRateDen = 80;
const LONGLONG kFrameDuration100ns = 800000;

// Selects the NV12-to-ARGB implementation. VideoProcessor stays on the GPU (the converted ARGB
// only crosses GPU->CPU once at the very end) while libyuv reads raw NV12 back to system memory
// before converting. Default to VP since chroma upsampling artifacts on decode are typically
// much milder than the subsampling artifacts on encode.
constexpr bool kUseLibyuvForChromaConversion = false;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderH264MF> VideoDecoderH264MF::create()
{
    std::unique_ptr<VideoDecoderH264MF> instance(new VideoDecoderH264MF());
    if (!instance->initialize())
        return nullptr;
    return instance;
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoDecoderH264MF::isHardwareSupported()
{
    if (!mf::isRuntimeAvailable())
        return false;

    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };
    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    _com_error error = mf::enumTransforms(MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &input_info, nullptr, &activate_arr,
        &count);
    if (FAILED(error.Error()) || count == 0)
        return false;

    for (UINT32 i = 0; i < count; ++i)
        activate_arr.get()[i]->Release();
    return true;
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264MF::VideoDecoderH264MF()
{
    // All real work happens in initialize(); see create().
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264MF::~VideoDecoderH264MF()
{
    destroyDecoder();

    if (mf_started_)
        mf::shutdown();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::initialize()
{
    if (!isHardwareSupported())
    {
        LOG(WARNING) << "No hardware H264 decoder MFT available";
        return false;
    }

    _com_error error = mf::startup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFStartup failed:" << error;
        return false;
    }
    mf_started_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::decode(const proto::video::Packet& packet, Frame* frame)
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

    if (!waitForEvent(METransformNeedInput))
        return false;

    if (!feedInput(packet.data(), sample_time))
        return false;

    if (!waitForEvent(METransformHaveOutput))
        return false;

    ComPtr<IMFSample> sample;
    if (!readOutput(&sample))
        return false;

    if (!sample)
        return false;

    if (!copySampleToFrame(sample.Get(), packet, frame))
        return false;

    ++frame_counter_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::createDecoder(const QSize& size)
{
    d3d_ = D3D11VideoContext::create();
    if (!d3d_)
    {
        LOG(ERROR) << "Failed to create D3D11 context";
        return false;
    }

    if (!activateHardwareMft())
        return false;

    // Attributes must be retrieved (and the async lock cleared) before any other method is called
    // on a hardware MFT; otherwise GetStreamIDs and friends return MF_E_TRANSFORM_ASYNC_LOCKED.
    ComPtr<IMFAttributes> attrs;
    _com_error error = decoder_->GetAttributes(&attrs);
    if (FAILED(error.Error()) || !attrs)
    {
        LOG(ERROR) << "GetAttributes failed:" << error;
        return false;
    }
    attrs->SetUINT32(MF_LOW_LATENCY, TRUE);

    error = attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Failed to unlock async MFT:" << error;
        return false;
    }
    error = decoder_.As(&event_gen_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT does not expose IMFMediaEventGenerator:" << error;
        return false;
    }

    error = decoder_->GetStreamIDs(1, &input_stream_id_, 1, &output_stream_id_);
    if (error.Error() == E_NOTIMPL)
    {
        input_stream_id_ = 0;
        output_stream_id_ = 0;
        error = S_OK;
    }
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::GetStreamIDs failed:" << error;
        return false;
    }

    error = decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                     reinterpret_cast<ULONG_PTR>(d3d_->manager()));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT_MESSAGE_SET_D3D_MANAGER failed:" << error;
        return false;
    }

    if (!configureMediaTypes(size))
        return false;

    if (!refreshOutputDimensions())
        return false;

    MFT_OUTPUT_STREAM_INFO out_info;
    memset(&out_info, 0, sizeof(out_info));
    error = decoder_->GetOutputStreamInfo(output_stream_id_, &out_info);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "GetOutputStreamInfo failed:" << error;
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
void VideoDecoderH264MF::destroyDecoder()
{
    if (streaming_)
        endStreaming();

    vp_output_view_.Reset();
    vp_processor_.Reset();
    vp_enumerator_.Reset();
    argb_target_.Reset();
    argb_staging_.Reset();
    nv12_staging_.Reset();

    event_gen_.Reset();
    if (decoder_)
    {
        decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0);
        decoder_.Reset();
    }

    d3d_.reset();

    output_provides_samples_ = false;
    output_sample_size_ = 0;
    output_width_ = 0;
    output_height_ = 0;
    output_stride_ = 0;
    frame_counter_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::activateHardwareMft()
{
    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };

    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    _com_error error = mf::enumTransforms(MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, &input_info, nullptr, &activate_arr,
        &count);
    if (FAILED(error.Error()) || count == 0)
    {
        LOG(ERROR) << "No hardware H264 decoder MFT found";
        return false;
    }

    error = activate_arr.get()[0]->ActivateObject(IID_PPV_ARGS(&decoder_));

    for (UINT32 i = 0; i < count; ++i)
        activate_arr.get()[i]->Release();

    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFActivate::ActivateObject failed:" << error;
        decoder_.Reset();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::configureMediaTypes(const QSize& size)
{
    ComPtr<IMFMediaType> input_type;
    _com_error error = mf::createMediaType(&input_type);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateMediaType (input) failed:" << error;
        return false;
    }

    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(size.width()), static_cast<UINT32>(size.height()));
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, kAssumedFrameRateNum, kAssumedFrameRateDen);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    error = decoder_->SetInputType(input_stream_id_, input_type.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "SetInputType failed:" << error;
        return false;
    }

    return selectOutputType();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::selectOutputType()
{
    DWORD index = 0;
    while (true)
    {
        ComPtr<IMFMediaType> candidate;
        _com_error error = decoder_->GetOutputAvailableType(output_stream_id_, index, &candidate);
        if (error.Error() == MF_E_NO_MORE_TYPES)
            break;
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "GetOutputAvailableType failed:" << error;
            return false;
        }
        ++index;

        GUID subtype = GUID_NULL;
        candidate->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (subtype != MFVideoFormat_NV12)
            continue;

        error = decoder_->SetOutputType(output_stream_id_, candidate.Get(), 0);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "SetOutputType failed:" << error;
            return false;
        }
        return true;
    }

    LOG(ERROR) << "No NV12 output type available on H264 decoder";
    return false;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::beginStreaming()
{
    decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    streaming_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264MF::endStreaming()
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
bool VideoDecoderH264MF::refreshOutputDimensions()
{
    ComPtr<IMFMediaType> out_type;
    _com_error error = decoder_->GetOutputCurrentType(output_stream_id_, &out_type);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "GetOutputCurrentType failed:" << error;
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
bool VideoDecoderH264MF::allocateGpuResources(const QSize& size)
{
    if constexpr (kUseLibyuvForChromaConversion)
    {
        nv12_staging_ = d3d_->createStagingNv12Texture(size.width(), size.height());
        if (!nv12_staging_)
            return false;
        return true;
    }

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

    _com_error error = d3d_->videoDevice()->CreateVideoProcessorEnumerator(&content_desc, &vp_enumerator_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessorEnumerator failed:" << error;
        return false;
    }

    error = d3d_->videoDevice()->CreateVideoProcessor(vp_enumerator_.Get(), 0, &vp_processor_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessor failed:" << error;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov_desc;
    memset(&ov_desc, 0, sizeof(ov_desc));
    ov_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ov_desc.Texture2D.MipSlice = 0;

    error = d3d_->videoDevice()->CreateVideoProcessorOutputView(
        argb_target_.Get(), vp_enumerator_.Get(), &ov_desc, &vp_output_view_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessorOutputView failed:" << error;
        return false;
    }

    const RECT full_rect = { 0, 0, size.width(), size.height() };
    d3d_->videoContext()->VideoProcessorSetStreamSourceRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetStreamDestRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetOutputTargetRect(vp_processor_.Get(), TRUE, &full_rect);

    // Mirror the encoder's color space pinning - BT.601 Limited matches encoder VP output and
    // libyuv defaults so HW and SW decoder paths produce identical ARGB pixels.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE nv12_cs;
    memset(&nv12_cs, 0, sizeof(nv12_cs));
    nv12_cs.YCbCr_Matrix = 0;  // BT.601.
    nv12_cs.Nominal_Range = 2; // Limited range 16-235.
    d3d_->videoContext()->VideoProcessorSetStreamColorSpace(vp_processor_.Get(), 0, &nv12_cs);

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb_cs;
    memset(&rgb_cs, 0, sizeof(rgb_cs));
    rgb_cs.RGB_Range = 0; // Full range 0-255 (target ARGB).
    d3d_->videoContext()->VideoProcessorSetOutputColorSpace(vp_processor_.Get(), &rgb_cs);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::waitForEvent(MediaEventType expected)
{
    while (true)
    {
        ComPtr<IMFMediaEvent> event;
        _com_error error = event_gen_->GetEvent(0, &event);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "IMFMediaEventGenerator::GetEvent failed:" << error;
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
bool VideoDecoderH264MF::feedInput(const std::string& data, quint64 sample_time_100ns)
{
    const DWORD data_size = static_cast<DWORD>(data.size());

    ComPtr<IMFMediaBuffer> buffer;
    _com_error error = mf::createAlignedMemoryBuffer(data_size, MF_16_BYTE_ALIGNMENT, &buffer);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateAlignedMemoryBuffer failed:" << error;
        return false;
    }

    BYTE* dst = nullptr;
    error = buffer->Lock(&dst, nullptr, nullptr);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFMediaBuffer::Lock failed:" << error;
        return false;
    }
    memcpy(dst, data.data(), data_size);
    buffer->Unlock();
    buffer->SetCurrentLength(data_size);

    ComPtr<IMFSample> sample;
    error = mf::createSample(&sample);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateSample failed:" << error;
        return false;
    }
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(sample_time_100ns));
    sample->SetSampleDuration(kFrameDuration100ns);

    error = decoder_->ProcessInput(input_stream_id_, sample.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::ProcessInput failed:" << error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::readOutput(ComPtr<IMFSample>* out_sample)
{
    out_sample->Reset();

    MFT_OUTPUT_DATA_BUFFER out;
    memset(&out, 0, sizeof(out));
    out.dwStreamID = output_stream_id_;

    ComPtr<IMFSample> allocated_sample;
    ComPtr<IMFMediaBuffer> allocated_buffer;
    if (!output_provides_samples_)
    {
        _com_error error = mf::createSample(&allocated_sample);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "MFCreateSample (output) failed:" << error;
            return false;
        }
        error = mf::createAlignedMemoryBuffer(
            std::max<DWORD>(output_sample_size_, 1u), MF_16_BYTE_ALIGNMENT, &allocated_buffer);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "MFCreateAlignedMemoryBuffer (output) failed:" << error;
            return false;
        }
        allocated_sample->AddBuffer(allocated_buffer.Get());
        out.pSample = allocated_sample.Get();
    }

    DWORD status = 0;
    _com_error error = decoder_->ProcessOutput(0, 1, &out, &status);

    // Take ownership of refs the MFT may have written into the struct, regardless of error.
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

    if (error.Error() == MF_E_TRANSFORM_STREAM_CHANGE)
    {
        if (!selectOutputType() || !refreshOutputDimensions())
            return false;
        return readOutput(out_sample);
    }

    if (FAILED(error.Error()))
    {
        if (error.Error() != MF_E_TRANSFORM_NEED_MORE_INPUT)
            LOG(ERROR) << "IMFTransform::ProcessOutput failed:" << error;
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
bool VideoDecoderH264MF::copySampleToFrame(
    IMFSample* sample, const proto::video::Packet& packet, Frame* frame)
{
    ComPtr<IMFMediaBuffer> buffer;
    _com_error error = sample->GetBufferByIndex(0, &buffer);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFSample::GetBufferByIndex failed:" << error;
        return false;
    }

    ComPtr<IMFDXGIBuffer> dxgi_buffer;
    error = buffer.As(&dxgi_buffer);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Sample buffer is not DXGI-backed:" << error;
        return false;
    }

    ComPtr<ID3D11Texture2D> nv12_texture;
    error = dxgi_buffer->GetResource(IID_PPV_ARGS(&nv12_texture));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFDXGIBuffer::GetResource failed:" << error;
        return false;
    }

    UINT subresource = 0;
    dxgi_buffer->GetSubresourceIndex(&subresource);

    if constexpr (kUseLibyuvForChromaConversion)
    {
        // Pull raw NV12 from the decoder's pool texture into our CPU-readable staging copy,
        // then let libyuv handle NV12->ARGB on the CPU (matches the SW decoder path exactly).
        d3d_->deviceContext()->CopySubresourceRegion(nv12_staging_.Get(), 0, 0, 0, 0,
                                                    nv12_texture.Get(), subresource, nullptr);

        D3D11_MAPPED_SUBRESOURCE mapped;
        error = d3d_->deviceContext()->Map(nv12_staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "Map nv12_staging_ failed:" << error;
            return false;
        }

        const int y_stride = static_cast<int>(mapped.RowPitch);
        const int uv_stride = y_stride;
        const quint8* y_plane = static_cast<const quint8*>(mapped.pData);
        const quint8* uv_plane = y_plane + y_stride * output_height_;

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

        d3d_->deviceContext()->Unmap(nv12_staging_.Get(), 0);
        return ok;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv_desc;
    memset(&iv_desc, 0, sizeof(iv_desc));
    iv_desc.FourCC = 0;
    iv_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    iv_desc.Texture2D.MipSlice = 0;
    iv_desc.Texture2D.ArraySlice = subresource;

    ComPtr<ID3D11VideoProcessorInputView> input_view;
    error = d3d_->videoDevice()->CreateVideoProcessorInputView(
        nv12_texture.Get(), vp_enumerator_.Get(), &iv_desc, &input_view);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessorInputView failed:" << error;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream;
    memset(&stream, 0, sizeof(stream));
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.pInputSurface = input_view.Get();

    error = d3d_->videoContext()->VideoProcessorBlt(vp_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "VideoProcessorBlt failed:" << error;
        return false;
    }

    d3d_->deviceContext()->CopyResource(argb_staging_.Get(), argb_target_.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    error = d3d_->deviceContext()->Map(argb_staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Map argb_staging_ failed:" << error;
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
