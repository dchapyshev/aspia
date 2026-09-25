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

#include "base/codec/video_encoder_h264_mf.h"

#include "base/codec/mf_runtime.h"
#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/win/scoped_co_mem.h"
#include "proto/desktop_video.h"

#include <libyuv/convert_from_argb.h>

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

const UINT32 kMaxRefFrames = 1;

// The transform reads an input texture after ProcessInput has returned, so consecutive frames go
// to different textures.
const size_t kInputTextureCount = 3;

// After this many failed frames in a row the caller falls back to a software codec.
const int kMaxFailures = 3;

// Selects the ARGB-to-NV12 implementation. libyuv handles subpixel-rendered text without
// color fringes; the GPU VideoProcessor path is faster but exhibits visible chroma artifacts
// on small text with many drivers. Flip to false to fall back to the VP path.
constexpr bool kUseLibyuvForChromaConversion = true;

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
        // Different vendor MFTs accept different subsets of CODECAPI_* hints. Failures are
        // expected and non-fatal - the encoder simply keeps its default for that attribute.
        LOG(WARNING) << "ICodecAPI::SetValue (uint32) skipped:" << error;
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
        LOG(WARNING) << "ICodecAPI::SetValue (bool) skipped:" << error;
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

//--------------------------------------------------------------------------------------------------
// The first hardware transform of the vendor with DXGI id |vendor_id|, or nullptr.
IMFActivate* findActivate(IMFActivate** activate_arr, UINT32 count, UINT vendor_id)
{
    for (UINT32 i = 0; i < count; ++i)
    {
        // The value looks like "VEN_10DE".
        WCHAR vendor[32] = { 0 };
        activate_arr[i]->GetString(
            MFT_ENUM_HARDWARE_VENDOR_ID_Attribute, vendor, ARRAYSIZE(vendor), nullptr);

        const QString id = QString::fromWCharArray(vendor).section('_', 1, 1);
        if (id.toUInt(nullptr, 16) == vendor_id)
            return activate_arr[i];
    }

    return nullptr;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoderH264MF> VideoEncoderH264MF::create(IDXGIAdapter* adapter)
{
    std::unique_ptr<VideoEncoderH264MF> instance(new VideoEncoderH264MF());
    instance->adapter_ = adapter;
    if (!instance->initialize())
        return nullptr;
    return instance;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264MF::VideoEncoderH264MF()
    : VideoEncoder(proto::video::ENCODING_H264)
{
    // All real work happens in initialize(); see create().
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264MF::~VideoEncoderH264MF()
{
    destroyEncoder();

    if (mf_started_)
        mf::shutdown();
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::initialize()
{
    if (!mf::isRuntimeAvailable())
    {
        LOG(ERROR) << "Media Foundation runtime not available on this system";
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
// static
bool VideoEncoderH264MF::isHardwareSupported(IDXGIAdapter* adapter)
{
    if (!mf::isRuntimeAvailable())
        return false;

    // MFTEnumEx requires Media Foundation to be initialized; bracket the probe so callers can
    // ask about HW support before any encoder/decoder has called MFStartup for its own use.
    _com_error error = mf::startup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(error.Error()))
        return false;

    MFT_REGISTER_TYPE_INFO output_info = { MFMediaType_Video, MFVideoFormat_H264 };
    ScopedCoMem<IMFActivate*> activate_arr;
    UINT32 count = 0;

    error = mf::enumTransforms(MFT_CATEGORY_VIDEO_ENCODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
        nullptr, &output_info, &activate_arr, &count);

    bool supported = SUCCEEDED(error.Error()) && count > 0;
    if (supported)
    {
        if (adapter)
        {
            DXGI_ADAPTER_DESC desc = {};
            adapter->GetDesc(&desc);
            supported = findActivate(activate_arr.get(), count, desc.VendorId) != nullptr;
        }

        for (UINT32 i = 0; i < count; ++i)
            activate_arr.get()[i]->Release();
    }

    mf::shutdown();
    return supported;
}

//--------------------------------------------------------------------------------------------------
VideoEncoder::Result VideoEncoderH264MF::encode(const Frame* frame, proto::video::Packet* packet)
{
    if (!mf_started_ || !frame || !packet)
        return Result::PERMANENT_ERROR;

    packet->set_encoding(proto::video::ENCODING_H264);

    bool is_key_frame = isKeyFrameRequired();

    // H264 is YUV 4:2:0 and cannot carry an odd width or height, while a screen can have one (a
    // virtual machine window of any size). The last odd column and row are left out.
    const QSize new_size(frame->size().width() & ~1, frame->size().height() & ~1);
    if (last_size_ != new_size)
    {
        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(new_size.width());
        video_rect->set_height(new_size.height());

        destroyEncoder();
        if (!createEncoder(new_size))
        {
            // HW encoders refuse to initialize when the frame size is outside their supported
            // profile/level (e.g. too small for some Intel SKUs, above 4K on others). This is the
            // signal for the caller to fall back to a software codec.
            LOG(ERROR) << "Unable to create H264 encoder for " << new_size;
            return Result::PERMANENT_ERROR;
        }

        last_size_ = new_size;
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
        Region updated_region;
        for (const auto& rect : frame->constUpdatedRegion())
            updated_region += alignRect(rect);
        updated_region.intersect(image_rect);

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
        return failure();

    if (!waitForEvent(METransformNeedInput))
        return failure();

    if (force_key_frame_next_)
    {
        setUint32CodecAttr(codec_api_.Get(), CODECAPI_AVEncVideoForceKeyFrame, 1);
        force_key_frame_next_ = false;
    }

    const quint64 sample_time = frame_counter_ * static_cast<quint64>(kFrameDuration100ns);
    ComPtr<IMFSample> input_sample;
    if (!buildInputSample(sample_time, &input_sample))
        return failure();

    _com_error error = encoder_->ProcessInput(input_stream_id_, input_sample.Get(), 0);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFTransform::ProcessInput failed:" << error;
        return failure();
    }
    next_input_ = (next_input_ + 1) % input_textures_.size();

    if (!waitForEvent(METransformHaveOutput))
        return failure();

    bool output_is_key = false;
    if (!readOutput(packet, &output_is_key))
        return failure();

    if (is_key_frame || output_is_key)
        packet->set_flags(proto::video::PACKET_FLAG_IS_KEY_FRAME);

    ++frame_counter_;
    failures_ = 0;
    setKeyFrameRequired(false);
    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
// The transform may still hold the frame it failed on, so it is recreated for the next frame.
VideoEncoder::Result VideoEncoderH264MF::failure()
{
    destroyEncoder();
    last_size_ = QSize();

    if (++failures_ < kMaxFailures)
        return Result::TEMPORARY_ERROR;

    LOG(ERROR) << "H264 encoder failed" << failures_ << "times in a row";
    return Result::PERMANENT_ERROR;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264MF::setBandwidth(qint64 bandwidth)
{
    const quint32 prev_min_quantizer = min_quantizer_;
    const quint32 prev_max_quantizer = max_quantizer_;
    const quint32 prev_common_quality = common_quality_;

    if (bandwidth <= 0)
    {
        // Bandwidth is not measured yet - fall back to the defaults baked into the header.
        min_quantizer_ = 16;
        max_quantizer_ = 28;
        common_quality_ = 85;
        target_bitrate_bps_ = 5 * 1000 * 1000;
    }
    else
    {
        // Keep ~15% headroom under the measured capacity.
        const quint64 budget_bps = static_cast<quint64>(bandwidth) * 8 * 85 / 100;
        target_bitrate_bps_ =
            static_cast<quint32>(std::clamp<quint64>(budget_bps, 200 * 1000, 20 * 1000 * 1000));

        // H264 is more efficient than VP at the same QP, so the bands are tighter. Loosen the
        // ceiling on low-bandwidth links so the encoder can compress aggressively without ever
        // dropping frames; tighten the floor on high-bandwidth links to spend bits on quality.
        // |common_quality_| is the central target the quality rate mode aims for; the QP bounds
        // only fence it in. Lower it on narrow links so the encoder leans toward smaller frames,
        // raise it on fast links so it spends the available bits on a sharper picture.
        if (bandwidth < 70 * 1024) // < 70 KB/s
        {
            min_quantizer_ = 24;
            max_quantizer_ = 46;
            common_quality_ = 40;
        }
        else if (bandwidth < 150 * 1024) // < 150 KB/s
        {
            min_quantizer_ = 22;
            max_quantizer_ = 42;
            common_quality_ = 50;
        }
        else if (bandwidth < 500 * 1024) // < 500 KB/s
        {
            min_quantizer_ = 20;
            max_quantizer_ = 38;
            common_quality_ = 60;
        }
        else if (bandwidth < 2 * 1024 * 1024) // < 2 MB/s
        {
            min_quantizer_ = 18;
            max_quantizer_ = 32;
            common_quality_ = 80;
        }
        else
        {
            min_quantizer_ = 14;
            max_quantizer_ = 26;
            common_quality_ = 90;
        }
    }

    // No live encoder yet - the new values will be picked up by configureCodecApi() when encode()
    // lazily creates one.
    if (!codec_api_)
        return;

    // This MFT only honors rate-control parameters at creation time; SetValue on a streaming
    // encoder is silently dropped.
    const bool params_changed = prev_min_quantizer != min_quantizer_ ||
        prev_max_quantizer != max_quantizer_ || prev_common_quality != common_quality_;
    if (!params_changed)
        return;

    LOG(INFO) << "Bandwidth changed. Recreating H264 encoder (quality:" << common_quality_
              << "min_qp:" << min_quantizer_ << "max_qp:" << max_quantizer_ << ")";

    destroyEncoder();
    last_size_ = QSize();
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::createEncoder(const QSize& size)
{
    d3d_ = D3D11VideoContext::create(adapter_.Get());
    if (!d3d_)
    {
        LOG(ERROR) << "Failed to create D3D11 context";
        return false;
    }

    if (!selectHardwareMft())
        return false;

    // Async MFT contract: every method on IMFTransform (except GetAttributes) returns
    // MF_E_TRANSFORM_ASYNC_LOCKED until MF_TRANSFORM_ASYNC_UNLOCK is set. Unlock first, then talk.
    ComPtr<IMFAttributes> attrs;
    _com_error error = encoder_->GetAttributes(&attrs);
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

    error = encoder_->GetStreamIDs(1, &input_stream_id_, 1, &output_stream_id_);
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

    configureCodecApi();

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
void VideoEncoderH264MF::destroyEncoder()
{
    if (streaming_)
        endStreaming();

    vp_input_view_.Reset();
    vp_output_views_.clear();
    vp_processor_.Reset();
    vp_enumerator_.Reset();
    argb_texture_.Reset();
    staging_texture_.Reset();
    input_textures_.clear();
    next_input_ = 0;

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
    input_credits_ = 0;
    output_ready_ = 0;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::selectHardwareMft()
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

    // A transform of another adapter refuses the device manager of ours.
    IMFActivate* activate = findActivate(activate_arr.get(), count, d3d_->adapterDesc().VendorId);
    if (!activate)
        activate = activate_arr.get()[0];

    WCHAR name[256] = { 0 };
    activate->GetString(MFT_FRIENDLY_NAME_Attribute, name, ARRAYSIZE(name), nullptr);
    LOG(INFO) << "Using H264 encoder:" << QString::fromWCharArray(name);

    error = activate->ActivateObject(IID_PPV_ARGS(&encoder_));

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
bool VideoEncoderH264MF::configureMediaTypes(const QSize& size)
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
    output_type->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate_bps_);
    output_type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    // High profile: 8x8 transform + custom quant matrices give ~5-10% better compression at the
    // same quality compared to Main. CABAC stays enabled below; both profiles support it.
    output_type->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
    // VUI color metadata - declares the matrix and range we actually feed the encoder so any
    // decoder reproduces matching colors. BT.601 Limited is what libyuv::ARGBToNV12 outputs.
    output_type->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT601);
    output_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
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
void VideoEncoderH264MF::configureCodecApi()
{
    ICodecAPI* api = codec_api_.Get();
    setUint32CodecAttr(api, CODECAPI_AVEncCommonRateControlMode, eAVEncCommonRateControlMode_Quality);
    setUint32CodecAttr(api, CODECAPI_AVEncCommonQuality, common_quality_);
    setUint32CodecAttr(api, CODECAPI_AVEncCommonMeanBitRate, target_bitrate_bps_);
    setUint32CodecAttr(api, CODECAPI_AVEncCommonQualityVsSpeed, 90);
    setUint32CodecAttr(api, CODECAPI_AVEncMPVDefaultBPictureCount, 0);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMaxNumRefFrame, kMaxRefFrames);
    setBoolCodecAttr(api, CODECAPI_AVEncCommonLowLatency, true);
    setBoolCodecAttr(api, CODECAPI_AVEncH264CABACEnable, true);
    setUint32CodecAttr(api, CODECAPI_AVScenarioInfo, eAVScenarioInfo_DisplayRemoting);
    setUint32CodecAttr(api, CODECAPI_AVEncMPVGOPSize, 60000);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMinQP, min_quantizer_);
    setUint32CodecAttr(api, CODECAPI_AVEncVideoMaxQP, max_quantizer_);
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::beginStreaming()
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
bool VideoEncoderH264MF::endStreaming()
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
bool VideoEncoderH264MF::allocateGpuResources(const QSize& size)
{
    for (size_t i = 0; i < kInputTextureCount; ++i)
    {
        ComPtr<ID3D11Texture2D> texture = d3d_->createNv12Texture(size.width(), size.height());
        if (!texture)
            return false;

        input_textures_.push_back(std::move(texture));
    }

    if constexpr (kUseLibyuvForChromaConversion)
    {
        staging_texture_ = d3d_->createStagingNv12Texture(
            size.width(), size.height(), D3D11_CPU_ACCESS_WRITE);
        if (!staging_texture_)
            return false;

        return true;
    }

    argb_texture_ = d3d_->createDefaultArgbTexture(size.width(), size.height());
    if (!argb_texture_)
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

    for (const auto& texture : input_textures_)
    {
        ComPtr<ID3D11VideoProcessorOutputView> view;
        error = d3d_->videoDevice()->CreateVideoProcessorOutputView(
            texture.Get(), vp_enumerator_.Get(), &ov_desc, &view);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "CreateVideoProcessorOutputView failed:" << error;
            return false;
        }

        vp_output_views_.push_back(std::move(view));
    }

    const RECT full_rect = { 0, 0, size.width(), size.height() };
    d3d_->videoContext()->VideoProcessorSetStreamSourceRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetStreamDestRect(vp_processor_.Get(), 0, TRUE, &full_rect);
    d3d_->videoContext()->VideoProcessorSetOutputTargetRect(vp_processor_.Get(), TRUE, &full_rect);

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE rgb_cs;
    memset(&rgb_cs, 0, sizeof(rgb_cs));
    rgb_cs.RGB_Range = 0; // Full range 0-255 (desktop ARGB).
    d3d_->videoContext()->VideoProcessorSetStreamColorSpace(vp_processor_.Get(), 0, &rgb_cs);

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE nv12_cs;
    memset(&nv12_cs, 0, sizeof(nv12_cs));
    nv12_cs.YCbCr_Matrix = 0;  // BT.601 (matches libyuv default).
    nv12_cs.Nominal_Range = 2; // Limited range 16-235.
    d3d_->videoContext()->VideoProcessorSetOutputColorSpace(vp_processor_.Get(), &nv12_cs);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::uploadArgbAndConvert(const Frame* frame)
{
    if (input_textures_.empty())
    {
        LOG(ERROR) << "GPU resources not allocated";
        return false;
    }

    ID3D11Texture2D* texture = input_textures_[next_input_].Get();

    if constexpr (kUseLibyuvForChromaConversion)
    {
        // The rows are written with the pitch the driver reports, so the driver copies nothing on
        // the CPU. Intel HD Graphics of 2016 overruns the UV plane in UpdateSubresource of NV12.
        D3D11_MAPPED_SUBRESOURCE mapped;
        _com_error error = d3d_->deviceContext()->Map(
            staging_texture_.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
        if (FAILED(error.Error()))
        {
            LOG(ERROR) << "Map staging_texture_ failed:" << error;
            return false;
        }

        const int stride = static_cast<int>(mapped.RowPitch);
        quint8* y_plane = static_cast<quint8*>(mapped.pData);
        quint8* uv_plane = y_plane + stride * last_size_.height();

        libyuv::ARGBToNV12(frame->frameData(), frame->stride(), y_plane, stride, uv_plane, stride,
                           last_size_.width(), last_size_.height());

        d3d_->deviceContext()->Unmap(staging_texture_.Get(), 0);
        d3d_->deviceContext()->CopyResource(texture, staging_texture_.Get());
        return true;
    }

    d3d_->deviceContext()->UpdateSubresource(
        argb_texture_.Get(), 0, nullptr, frame->frameData(), static_cast<UINT>(frame->stride()), 0);

    D3D11_VIDEO_PROCESSOR_STREAM stream;
    memset(&stream, 0, sizeof(stream));
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = vp_input_view_.Get();

    _com_error error = d3d_->videoContext()->VideoProcessorBlt(
        vp_processor_.Get(), vp_output_views_[next_input_].Get(), 0, 1, &stream);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "VideoProcessorBlt failed:" << error;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::buildInputSample(quint64 sample_time_100ns, ComPtr<IMFSample>* out)
{
    ComPtr<IMFMediaBuffer> buffer;
    _com_error error = mf::createDxgiSurfaceBuffer(
        __uuidof(ID3D11Texture2D), input_textures_[next_input_].Get(), 0, FALSE, &buffer);
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
// The transform sends its events in its own order. Intel asks for the next frame before it
// reports the output of the previous one, so events are counted and consumed when needed.
bool VideoEncoderH264MF::waitForEvent(MediaEventType expected)
{
    int& counter = (expected == METransformNeedInput) ? input_credits_ : output_ready_;

    while (counter == 0)
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

        if (type == METransformNeedInput)
            ++input_credits_;
        else if (type == METransformHaveOutput)
            ++output_ready_;
        else if (type == MEError)
        {
            HRESULT status = S_OK;
            event->GetStatus(&status);
            LOG(ERROR) << "MFT reported an error:" << _com_error(status);
            return false;
        }
    }

    --counter;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MF::readOutput(proto::video::Packet* packet, bool* is_key_frame_out)
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
