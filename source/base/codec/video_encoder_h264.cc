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

#include "base/codec/video_encoder_h264.h"

#include "base/codec/mf_runtime.h"
#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/win/scoped_co_mem.h"
#include "proto/desktop_video.h"

#include <comdef.h>
#include <mferror.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {

// Target encode cadence matches the VP path so the timestamps fed to the encoder
// are consistent with what the rate-control machinery expects.
const UINT32 kTargetFrameRateNum = 1000;
const UINT32 kTargetFrameRateDen = 80;
const LONGLONG kFrameDuration100ns = 800000;

const UINT32 kInitialBitrateBps = 1000 * 1000;
const UINT32 kMaxRefFrames = 1;

//--------------------------------------------------------------------------------------------------
bool setUint32CodecAttr(ICodecAPI* api, REFGUID key, UINT32 value)
{
    VARIANT var;
    VariantInit(&var);
    var.vt = VT_UI4;
    var.ulVal = value;
    _com_error error = api->SetValue(&key, &var);
    VariantClear(&var);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "ICodecAPI::SetValue failed:" << error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool setBoolCodecAttr(ICodecAPI* api, REFGUID key, bool value)
{
    VARIANT var;
    VariantInit(&var);
    var.vt = VT_BOOL;
    var.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    _com_error error = api->SetValue(&key, &var);
    VariantClear(&var);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "ICodecAPI::SetValue failed:" << error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
int roundToTwosMultiple(int x)
{
    return x & ~1;
}

//--------------------------------------------------------------------------------------------------
QRect alignRect(const QRect& rect)
{
    int x = roundToTwosMultiple(rect.left());
    int y = roundToTwosMultiple(rect.top());
    int right = roundToTwosMultiple(rect.right() + 1);
    int bottom = roundToTwosMultiple(rect.bottom() + 1);
    return QRect(QPoint(x, y), QPoint(right + 1, bottom + 1));
}

} // namespace

//--------------------------------------------------------------------------------------------------
VideoEncoderH264::VideoEncoderH264()
    : VideoEncoder(proto::video::ENCODING_H264)
{
    // H264 produces sharper output at higher QP than VP at the same number; tune defaults.
    min_quantizer_ = 18;
    max_quantizer_ = 40;

    if (!mf::isRuntimeAvailable())
    {
        LOG(ERROR) << "Media Foundation runtime not available on this system";
        return;
    }

    _com_error error = mf::startup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFStartup failed:" << error;
        return;
    }
    mf_started_ = true;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264::~VideoEncoderH264()
{
    destroyEncoder();

    if (mf_started_)
        mf::shutdown();
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::encode(const Frame* frame, proto::video::Packet* packet)
{
    if (!mf_started_ || !frame || !packet)
        return false;

    packet->set_encoding(proto::video::ENCODING_H264);

    bool is_key_frame = isKeyFrameRequired();

    if (last_size_ != frame->size())
    {
        last_size_ = frame->size();

        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(last_size_.width());
        video_rect->set_height(last_size_.height());

        destroyEncoder();
        if (!createEncoder(last_size_))
        {
            LOG(ERROR) << "Unable to create H264 encoder";
            return false;
        }

        is_key_frame = true;
    }

    QRect image_rect(QPoint(0, 0), last_size_);

    if (is_key_frame)
    {
        proto::video::Rect* dirty_rect = packet->add_dirty_rect();
        dirty_rect->set_x(0);
        dirty_rect->set_y(0);
        dirty_rect->set_width(last_size_.width());
        dirty_rect->set_height(last_size_.height());

        force_key_frame_next_ = true;
    }
    else
    {
        QRegion updated_region;
        for (const auto& rect : frame->constUpdatedRegion())
            updated_region += alignRect(rect);
        updated_region = updated_region.intersected(image_rect);

        for (const auto& rect : updated_region)
        {
            proto::video::Rect* dirty_rect = packet->add_dirty_rect();
            dirty_rect->set_x(rect.x());
            dirty_rect->set_y(rect.y());
            dirty_rect->set_width(rect.width());
            dirty_rect->set_height(rect.height());
        }
    }

    if (!uploadArgbAndConvert(frame))
        return false;

    if (!waitForEvent(METransformNeedInput))
        return false;

    if (force_key_frame_next_)
    {
        setUint32CodecAttr(codec_api_.Get(), CODECAPI_AVEncVideoForceKeyFrame, 1);
        force_key_frame_next_ = false;
    }

    const quint64 sample_time = frame_counter_ * static_cast<quint64>(kFrameDuration100ns);
    ComPtr<IMFSample> input_sample;
    if (!buildInputSample(sample_time, &input_sample))
        return false;

    _com_error error = encoder_->ProcessInput(input_stream_id_, input_sample.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::ProcessInput failed:" << error;
        return false;
    }

    if (!waitForEvent(METransformHaveOutput))
        return false;

    bool output_is_key = false;
    if (!readOutput(packet, &output_is_key))
        return false;

    if (is_key_frame || output_is_key)
        packet->set_flags(proto::video::PACKET_FLAG_IS_KEY_FRAME);

    ++frame_counter_;
    setKeyFrameRequired(false);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::createEncoder(const QSize& size)
{
    d3d_ = D3D11VideoContext::create();
    if (!d3d_)
    {
        LOG(ERROR) << "Failed to create D3D11 context";
        return false;
    }

    if (!selectHardwareMft())
        return false;

    _com_error error = encoder_->GetStreamIDs(1, &input_stream_id_, 1, &output_stream_id_);
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

    ComPtr<IMFAttributes> attrs;
    error = encoder_->GetAttributes(&attrs);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::GetAttributes failed:" << error;
        return false;
    }

    error = attrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Failed to unlock async MFT:" << error;
        return false;
    }

    // Hand the D3D11 device manager to the encoder so it consumes DXGI-backed samples directly.
    error = encoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(d3d_->manager()));
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT_MESSAGE_SET_D3D_MANAGER failed:" << error;
        return false;
    }

    error = encoder_.As(&event_gen_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT does not expose IMFMediaEventGenerator:" << error;
        return false;
    }

    error = encoder_.As(&codec_api_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT does not expose ICodecAPI:" << error;
        return false;
    }

    if (!configureCodecApi())
        return false;

    if (!configureMediaTypes(size))
        return false;

    MFT_OUTPUT_STREAM_INFO out_info;
    memset(&out_info, 0, sizeof(out_info));
    error = encoder_->GetOutputStreamInfo(output_stream_id_, &out_info);
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
void VideoEncoderH264::destroyEncoder()
{
    if (streaming_)
        endStreaming();

    vp_input_view_.Reset();
    vp_output_view_.Reset();
    vp_processor_.Reset();
    vp_enumerator_.Reset();
    argb_texture_.Reset();
    nv12_texture_.Reset();

    event_gen_.Reset();
    codec_api_.Reset();
    if (encoder_)
    {
        encoder_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0);
        encoder_.Reset();
    }

    d3d_.reset();

    output_provides_samples_ = false;
    output_sample_size_ = 0;
    frame_counter_ = 0;
    force_key_frame_next_ = false;
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoEncoderH264::isHardwareSupported()
{
    if (!mf::isRuntimeAvailable())
        return false;

    MFT_REGISTER_TYPE_INFO output_info = { MFMediaType_Video, MFVideoFormat_H264 };
    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    _com_error error = mf::enumTransforms(MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, nullptr, &output_info, &activate_arr,
        &count);
    if (FAILED(error.Error()) || count == 0)
        return false;

    for (UINT32 i = 0; i < count; ++i)
        activate_arr.get()[i]->Release();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::selectHardwareMft()
{
    MFT_REGISTER_TYPE_INFO output_info = { MFMediaType_Video, MFVideoFormat_H264 };

    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    _com_error error = mf::enumTransforms(MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, nullptr, &output_info, &activate_arr,
        &count);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFTEnumEx failed:" << error;
        return false;
    }

    if (count == 0)
    {
        LOG(ERROR) << "No hardware H264 encoders available";
        return false;
    }

    error = activate_arr.get()[0]->ActivateObject(IID_PPV_ARGS(&encoder_));

    for (UINT32 i = 0; i < count; ++i)
        activate_arr.get()[i]->Release();

    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFActivate::ActivateObject failed:" << error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::configureMediaTypes(const QSize& size)
{
    // Output type must be set before input type for encoders.
    ComPtr<IMFMediaType> output_type;
    _com_error error = mf::createMediaType(&output_type);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateMediaType (output) failed:" << error;
        return false;
    }

    output_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    output_type->SetUINT32(MF_MT_AVG_BITRATE, kInitialBitrateBps);
    output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    // High profile: 8x8 transform + custom quant matrices give ~5-10% better compression at the
    // same quality compared to Main. CABAC stays enabled below; both profiles support it.
    output_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
    MFSetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(size.width()), static_cast<UINT32>(size.height()));
    MFSetAttributeRatio(output_type.Get(), MF_MT_FRAME_RATE, kTargetFrameRateNum, kTargetFrameRateDen);
    MFSetAttributeRatio(output_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    error = encoder_->SetOutputType(output_stream_id_, output_type.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "SetOutputType failed:" << error;
        return false;
    }

    ComPtr<IMFMediaType> input_type;
    error = mf::createMediaType(&input_type);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateMediaType (input) failed:" << error;
        return false;
    }

    input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    input_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE,
                       static_cast<UINT32>(size.width()), static_cast<UINT32>(size.height()));
    MFSetAttributeRatio(input_type.Get(), MF_MT_FRAME_RATE, kTargetFrameRateNum, kTargetFrameRateDen);
    MFSetAttributeRatio(input_type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    error = encoder_->SetInputType(input_stream_id_, input_type.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "SetInputType failed:" << error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::configureCodecApi()
{
    ICodecAPI* api = codec_api_.Get();

    // LowDelayVBR is single-pass without look-ahead - the right mode for real-time encoding.
    // Bitrate adapts but stays bounded by the mean target; pure Quality mode could spike to many
    // megabits on a full screen redraw and flood the network.
    setUint32CodecAttr(api, CODECAPI_AVEncCommonRateControlMode,
                       eAVEncCommonRateControlMode_LowDelayVBR);
    setUint32CodecAttr(api, CODECAPI_AVEncCommonMeanBitRate, kInitialBitrateBps);
    setUint32CodecAttr(api, CODECAPI_AVEncMPVDefaultBPictureCount, 0);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMaxNumRefFrame, kMaxRefFrames);
    setBoolCodecAttr(api, CODECAPI_AVEncCommonLowLatency, true);
    setBoolCodecAttr(api, CODECAPI_AVEncH264CABACEnable, true);
    setUint32CodecAttr(api, CODECAPI_AVScenarioInfo, eAVScenarioInfo_DisplayRemoting);
    // Reliable transport makes periodic IDRs unnecessary for error recovery; a long GOP keeps the
    // bitrate flat. Keyframes are emitted on demand via CODECAPI_AVEncVideoForceKeyFrame.
    setUint32CodecAttr(api, CODECAPI_AVEncMPVGOPSize, 60000);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMinQP, min_quantizer_);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMaxQP, max_quantizer_);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::beginStreaming()
{
    _com_error error = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT_MESSAGE_COMMAND_FLUSH failed:" << error;
        return false;
    }

    error = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT_MESSAGE_NOTIFY_BEGIN_STREAMING failed:" << error;
        return false;
    }

    error = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFT_MESSAGE_NOTIFY_START_OF_STREAM failed:" << error;
        return false;
    }

    streaming_ = true;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::endStreaming()
{
    if (encoder_)
    {
        encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
    streaming_ = false;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::allocateGpuResources(const QSize& size)
{
    argb_texture_ = d3d_->createDynamicArgbTexture(size.width(), size.height());
    if (!argb_texture_)
        return false;

    nv12_texture_ = d3d_->createNv12Texture(size.width(), size.height());
    if (!nv12_texture_)
        return false;

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;
    memset(&content_desc, 0, sizeof(content_desc));
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputFrameRate.Numerator = kTargetFrameRateNum;
    content_desc.InputFrameRate.Denominator = kTargetFrameRateDen;
    content_desc.InputWidth = static_cast<UINT>(size.width());
    content_desc.InputHeight = static_cast<UINT>(size.height());
    content_desc.OutputFrameRate.Numerator = kTargetFrameRateNum;
    content_desc.OutputFrameRate.Denominator = kTargetFrameRateDen;
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

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC iv_desc;
    memset(&iv_desc, 0, sizeof(iv_desc));
    iv_desc.FourCC = 0;
    iv_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    iv_desc.Texture2D.MipSlice = 0;
    iv_desc.Texture2D.ArraySlice = 0;

    error = d3d_->videoDevice()->CreateVideoProcessorInputView(
        argb_texture_.Get(), vp_enumerator_.Get(), &iv_desc, &vp_input_view_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessorInputView failed:" << error;
        return false;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ov_desc;
    memset(&ov_desc, 0, sizeof(ov_desc));
    ov_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    ov_desc.Texture2D.MipSlice = 0;

    error = d3d_->videoDevice()->CreateVideoProcessorOutputView(
        nv12_texture_.Get(), vp_enumerator_.Get(), &ov_desc, &vp_output_view_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateVideoProcessorOutputView failed:" << error;
        return false;
    }

    // Restrict color conversion to a single full-frame stream by default.
    const RECT full_rect = { 0, 0, size.width(), size.height() };
    d3d_->videoContext()->VideoProcessorSetStreamSourceRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetStreamDestRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetOutputTargetRect(vp_processor_.Get(), TRUE, &full_rect);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::uploadArgbAndConvert(const Frame* frame)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    _com_error error = d3d_->deviceContext()->Map(argb_texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "Map argb_texture_ failed:" << error;
        return false;
    }

    const quint8* src = frame->frameData();
    const int src_stride = frame->stride();
    const int row_bytes = last_size_.width() * 4;
    const int height = last_size_.height();
    quint8* dst = static_cast<quint8*>(mapped.pData);

    if (src_stride == static_cast<int>(mapped.RowPitch) && src_stride == row_bytes)
    {
        memcpy(dst, src, static_cast<size_t>(row_bytes) * height);
    }
    else
    {
        for (int y = 0; y < height; ++y)
            memcpy(dst + y * mapped.RowPitch, src + y * src_stride, row_bytes);
    }

    d3d_->deviceContext()->Unmap(argb_texture_.Get(), 0);

    D3D11_VIDEO_PROCESSOR_STREAM stream;
    memset(&stream, 0, sizeof(stream));
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = vp_input_view_.Get();

    error = d3d_->videoContext()->VideoProcessorBlt(vp_processor_.Get(), vp_output_view_.Get(), 0, 1, &stream);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "VideoProcessorBlt failed:" << error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::buildInputSample(quint64 sample_time_100ns, ComPtr<IMFSample>* out)
{
    ComPtr<IMFMediaBuffer> buffer;
    _com_error error = mf::createDxgiSurfaceBuffer(
        __uuidof(ID3D11Texture2D), nv12_texture_.Get(), 0, FALSE, &buffer);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateDXGISurfaceBuffer failed:" << error;
        return false;
    }

    ComPtr<IMF2DBuffer> buffer_2d;
    if (SUCCEEDED(buffer.As(&buffer_2d)))
    {
        DWORD contiguous_length = 0;
        if (SUCCEEDED(buffer_2d->GetContiguousLength(&contiguous_length)))
            buffer->SetCurrentLength(contiguous_length);
    }

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

    *out = std::move(sample);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::waitForEvent(MediaEventType expected)
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
        error = event->GetType(&type);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "IMFMediaEvent::GetType failed:" << error;
            return false;
        }

        if (type == expected)
            return true;

        if (type == METransformDrainComplete || type == METransformMarker)
            continue;

        LOG(ERROR) << "Unexpected MFT event:" << type << "expected:" << expected;
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::readOutput(proto::video::Packet* packet, bool* is_key_frame_out)
{
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
    _com_error error = encoder_->ProcessOutput(0, 1, &out, &status);

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

    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::ProcessOutput failed:" << error;
        return false;
    }

    if (!sample)
    {
        LOG(ERROR) << "ProcessOutput returned no sample";
        return false;
    }

    UINT32 clean_point = 0;
    sample->GetUINT32(MFSampleExtension_CleanPoint, &clean_point);
    *is_key_frame_out = (clean_point != 0);

    ComPtr<IMFMediaBuffer> out_buffer;
    error = sample->ConvertToContiguousBuffer(&out_buffer);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "ConvertToContiguousBuffer failed:" << error;
        return false;
    }

    BYTE* src = nullptr;
    DWORD current_length = 0;
    error = out_buffer->Lock(&src, nullptr, &current_length);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFMediaBuffer::Lock (output) failed:" << error;
        return false;
    }

    if (encode_buffer_.capacity() < current_length)
        encode_buffer_.reserve(current_length);
    encode_buffer_.resize(current_length);
    memcpy(encode_buffer_.data(), src, current_length);

    out_buffer->Unlock();

    packet->set_data(std::move(encode_buffer_));
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::applyMinQuantizer()
{
    if (!codec_api_)
        return true;
    return setUint32CodecAttr(codec_api_.Get(), CODECAPI_AVEncVideoMinQP, min_quantizer_);
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264::applyMaxQuantizer()
{
    if (!codec_api_)
        return true;
    return setUint32CodecAttr(codec_api_.Get(), CODECAPI_AVEncVideoMaxQP, max_quantizer_);
}
