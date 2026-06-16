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
#include "base/win/scoped_co_mem.h"
#include "proto/desktop_video.h"

#include <comdef.h>
#include <mferror.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

const UINT32 kAssumedFrameRateNum = 1000;
const UINT32 kAssumedFrameRateDen = 80;
const LONGLONG kFrameDuration100ns = 800000;

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

    // Two HW paths exist on Windows:
    //   1. Vendor async MFT (MFT_ENUM_FLAG_HARDWARE | ASYNCMFT) - Intel QuickSync registers one,
    //      NVIDIA does not (their NVDEC is exposed through CUDA, not MF). AMD varies.
    //   2. Microsoft H.264 Video Decoder MFT (SYNCMFT) + D3D11 manager - the MFT delegates to
    //      DXVA2 internally, which routes to NVDEC/QuickSync/UVD inside the GPU driver. This is
    //      the universal path for NVIDIA and any other GPU that advertises a DXVA2 H.264 profile.
    // Either one counts as "HW decode available".
    _com_error error = mf::startup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(error.Error()))
        return false;

    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };
    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    error = mf::enumTransforms(MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        &input_info, nullptr, &activate_arr, &count);

    bool vendor_hw_available = SUCCEEDED(error.Error()) && count > 0;
    if (vendor_hw_available)
    {
        for (UINT32 i = 0; i < count; ++i)
            activate_arr.get()[i]->Release();
    }
    mf::shutdown();

    if (vendor_hw_available)
        return true;

    // No vendor async MFT - check if DXVA2 H.264 decode is available on this GPU. The MS H264
    // MFT will use it when given a D3D11 manager.
    auto d3d = D3D11VideoContext::create();
    return d3d && d3d->supportsH264Decode();
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
VideoDecoder::Result VideoDecoderH264MF::decode(const proto::video::Packet& packet)
{
    if (!mf_started_)
        return Result::PERMANENT_ERROR;

    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return Result::TEMPORARY_ERROR;
    }

    const QSize size = frameSize(packet);
    if (size.isEmpty())
    {
        LOG(ERROR) << "Unknown frame size";
        return Result::TEMPORARY_ERROR;
    }

    if (!decoder_ || last_size_ != size)
    {
        destroyDecoder();
        if (!createDecoder(size))
        {
            // HW MFT cannot be (re)initialized for this frame size or driver state - signal a
            // fallback to the software decoder instead of looping forever on the same failure.
            LOG(ERROR) << "Unable to create H264 decoder for" << size;
            return Result::PERMANENT_ERROR;
        }
        last_size_ = size;
    }

    const quint64 sample_time = frame_counter_ * static_cast<quint64>(kFrameDuration100ns);

    if (is_async_ && !waitForEvent(METransformNeedInput))
        return Result::TEMPORARY_ERROR;

    if (!feedInput(packet.data(), sample_time))
        return Result::TEMPORARY_ERROR;

    if (is_async_ && !waitForEvent(METransformHaveOutput))
        return Result::TEMPORARY_ERROR;

    // Sync MFTs (MS H.264 decoder) may need an extra packet before producing the first frame -
    // readOutput returns false with MF_E_TRANSFORM_NEED_MORE_INPUT in that case and the caller
    // will get the frame on the next packet.
    ComPtr<IMFSample> sample;
    if (!readOutput(&sample))
        return Result::TEMPORARY_ERROR;

    if (!sample)
        return Result::TEMPORARY_ERROR;

    if (!mapSampleToFrame(sample.Get(), size))
        return Result::TEMPORARY_ERROR;

    ++frame_counter_;
    return Result::SUCCESS;
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

    if (!activateMft())
        return false;

    ComPtr<IMFAttributes> attrs;
    _com_error error = decoder_->GetAttributes(&attrs);
    if (FAILED(error.Error()) || !attrs)
    {
        LOG(ERROR) << "GetAttributes failed:" << error;
        return false;
    }
    attrs->SetUINT32(MF_LOW_LATENCY, TRUE);

    // Vendor HW MFTs are async and must be unlocked before any further method call. The MS H.264
    // decoder MFT is sync - this attribute simply doesn't exist on it, which we treat as sync.
    UINT32 async_flag = 0;
    if (SUCCEEDED(attrs->GetUINT32(MF_TRANSFORM_ASYNC, &async_flag)) && async_flag)
    {
        is_async_ = true;
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

    if (!validateOutputType())
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

    if (!beginStreaming())
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264MF::destroyDecoder()
{
    if (streaming_)
        endStreaming();

    unmapStaging();
    nv12_staging_.Reset();

    event_gen_.Reset();
    if (decoder_)
    {
        decoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0);
        decoder_.Reset();
    }

    d3d_.reset();

    is_async_ = false;
    output_provides_samples_ = false;
    output_sample_size_ = 0;
    frame_counter_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MF::activateMft()
{
    MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };

    // Try vendor-supplied HW MFT first (Intel QuickSync registers one; NVIDIA does not). If none
    // is available, fall back to the Microsoft H.264 Decoder MFT - paired with the D3D11 manager
    // set later in createDecoder(), it uses DXVA2 internally and routes to whatever HW the GPU
    // exposes (NVDEC, etc.). Either way the actual decode happens on the GPU.
    const UINT32 flag_sets[] =
    {
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
    };

    for (UINT32 flags : flag_sets)
    {
        ScopedCoMem<IMFActivate*> activate_arr;
        UINT32 count = 0;

        _com_error error = mf::enumTransforms(MFT_CATEGORY_VIDEO_DECODER, flags,
            &input_info, nullptr, &activate_arr, &count);
        if (FAILED(error.Error()) || count == 0)
            continue;

        error = activate_arr.get()[0]->ActivateObject(IID_PPV_ARGS(&decoder_));
        for (UINT32 i = 0; i < count; ++i)
            activate_arr.get()[i]->Release();

        if (SUCCEEDED(error.Error()))
        {
            LOG(INFO) << "H264 decoder MFT activated"
                      << ((flags & MFT_ENUM_FLAG_HARDWARE) ? "(vendor HW)" : "(MS + DXVA2)");
            return true;
        }
        LOG(WARNING) << "IMFActivate::ActivateObject failed:" << error;
        decoder_.Reset();
    }

    LOG(ERROR) << "No H264 decoder MFT available";
    return false;
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
bool VideoDecoderH264MF::validateOutputType()
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

    return width > 0 && height > 0;
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
        if (!selectOutputType() || !validateOutputType())
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
bool VideoDecoderH264MF::mapSampleToFrame(IMFSample* sample, const QSize& size)
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

    // The decoder's output texture is allocated at the coded (macroblock-aligned) size, which can be
    // larger than the display size. The staging copy and the UV plane offset must match that coded
    // size, otherwise the NV12 copy mangles the UV plane.
    D3D11_TEXTURE2D_DESC src_desc;
    nv12_texture->GetDesc(&src_desc);

    // Release the previous frame's mapping before the GPU writes the staging texture again.
    unmapStaging();

    if (!nv12_staging_)
    {
        nv12_staging_ = d3d_->createStagingNv12Texture(
            static_cast<int>(src_desc.Width), static_cast<int>(src_desc.Height));
        if (!nv12_staging_)
        {
            LOG(ERROR) << "Failed to create NV12 staging texture";
            return false;
        }
    }

    // Pull raw NV12 from the decoder's pool texture into our CPU-readable staging copy. The mapping
    // is held open so frame_ can reference it directly.
    d3d_->deviceContext()->CopySubresourceRegion(nv12_staging_.Get(), 0, 0, 0, 0,
                                                nv12_texture.Get(), subresource, nullptr);

    D3D11_MAPPED_SUBRESOURCE mapped;
    error = d3d_->deviceContext()->Map(nv12_staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Map nv12_staging_ failed:" << error;
        return false;
    }
    nv12_mapped_ = true;

    const int src_stride = static_cast<int>(mapped.RowPitch);
    const quint8* y_plane = static_cast<const quint8*>(mapped.pData);
    const quint8* uv_plane = y_plane + src_stride * static_cast<int>(src_desc.Height);

    frame_.reset(YuvFormat::NV12, size);
    frame_.setPlane(0, y_plane, src_stride);
    frame_.setPlane(1, uv_plane, src_stride);

    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264MF::unmapStaging()
{
    if (nv12_mapped_)
    {
        d3d_->deviceContext()->Unmap(nv12_staging_.Get(), 0);
        nv12_mapped_ = false;
    }
}
