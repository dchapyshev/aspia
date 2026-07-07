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

#include "base/codec/video_encoder_h264_va.h"

#include <QRect>

#include <libyuv/convert_from_argb.h>
#include <va/va_drm.h>
#include <va/va_enc_h264.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "base/logging.h"
#include "base/audio/linux/late_binding_symbol_table.h"
#include "base/desktop/frame.h"
#include "base/desktop/region.h"
#include "proto/desktop_video.h"

namespace {

// DRM render nodes are numbered from 128; probing a handful covers multi-GPU systems.
const int kFirstRenderNode = 128;
const int kRenderNodeCount = 8;

const int kMacroblockSize = 16;

// frame_num is coded with this many bits in slice headers and wraps around accordingly.
const quint32 kLog2MaxFrameNum = 16;

// Timing hints for the rate control: the host captures at a fixed 30 fps cadence.
const qint32 kFrameRate = 30;

// Key frames only on demand (stream start, client request, size change). The interval is in
// frames; a spurious periodic key frame this rare is harmless.
const quint32 kIFrameInterval = 108000;

// Rate control averaging window and the target-to-peak bitrate ratio for VBR mode.
const quint32 kRateControlWindowMs = 1000;
const quint32 kVbrTargetPercentage = 90;

// H264 levels from A.3.1: the smallest level whose per-frame and per-second macroblock limits fit
// the stream is advertised in the SPS.
struct H264LevelLimit
{
    int level_idc;
    quint32 max_frame_mbs;
    quint32 max_mbs_per_sec;
};

const H264LevelLimit kLevelLimits[] =
{
    { 30, 1620,  40500   },
    { 31, 3600,  108000  },
    { 32, 5120,  216000  },
    { 40, 8192,  245760  },
    { 42, 8704,  522240  },
    { 50, 22080, 589824  },
    { 51, 36864, 983040  },
    { 52, 36864, 2073600 }
};

// The libva symbols we need, as an X-Macro list.
#define VA_SYMBOLS_LIST          \
    X(vaBeginPicture)              \
    X(vaCreateBuffer)              \
    X(vaCreateConfig)              \
    X(vaCreateContext)             \
    X(vaCreateImage)               \
    X(vaCreateSurfaces)            \
    X(vaDeriveImage)               \
    X(vaDestroyBuffer)             \
    X(vaDestroyConfig)             \
    X(vaDestroyContext)            \
    X(vaDestroyImage)              \
    X(vaDestroySurfaces)           \
    X(vaEndPicture)                \
    X(vaErrorStr)                  \
    X(vaGetConfigAttributes)       \
    X(vaInitialize)                \
    X(vaMapBuffer)                 \
    X(vaMaxNumEntrypoints)         \
    X(vaMaxNumProfiles)            \
    X(vaPutImage)                  \
    X(vaQueryConfigEntrypoints)    \
    X(vaQueryConfigProfiles)       \
    X(vaQueryVendorString)         \
    X(vaRenderPicture)             \
    X(vaSetErrorCallback)          \
    X(vaSetInfoCallback)           \
    X(vaSyncSurface)               \
    X(vaTerminate)                 \
    X(vaUnmapBuffer)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(VaSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(VaSymbolTable, sym)
VA_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(VaSymbolTable)

LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(VaSymbolTable, "libva.so.2")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(VaSymbolTable, sym)
VA_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(VaSymbolTable)

#define VA_DRM_SYMBOLS_LIST X(vaGetDisplayDRM)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(VaDrmSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(VaDrmSymbolTable, sym)
VA_DRM_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(VaDrmSymbolTable)

LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(VaDrmSymbolTable, "libva-drm.so.2")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(VaDrmSymbolTable, sym)
VA_DRM_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(VaDrmSymbolTable)

//--------------------------------------------------------------------------------------------------
VaSymbolTable* vaTable()
{
    static VaSymbolTable table;
    return &table;
}

//--------------------------------------------------------------------------------------------------
VaDrmSymbolTable* vaDrmTable()
{
    static VaDrmSymbolTable table;
    return &table;
}

#define VA(sym) LATESYM_GET(VaSymbolTable, vaTable(), sym)
#define VA_DRM(sym) LATESYM_GET(VaDrmSymbolTable, vaDrmTable(), sym)

//--------------------------------------------------------------------------------------------------
bool loadLibva()
{
    return vaTable()->load() && vaDrmTable()->load();
}

//--------------------------------------------------------------------------------------------------
// libva reports problems to stderr by default; route them into the log instead.
void vaMessageToLog(void* /* user_context */, const char* message)
{
    std::string text(message ? message : "");
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    LOG(INFO) << "libva:" << text.c_str();
}

//--------------------------------------------------------------------------------------------------
// Writes H264 header bitstreams (Annex B byte order, most significant bit first).
class BitWriter
{
public:
    BitWriter() = default;

    void writeBits(quint32 value, int count)
    {
        for (int i = count - 1; i >= 0; --i)
            writeBit((value >> i) & 1);
    }

    void writeUE(quint32 value)
    {
        const quint32 coded = value + 1;
        int bits = 0;
        for (quint32 tmp = coded; tmp; tmp >>= 1)
            ++bits;
        writeBits(0, bits - 1);
        writeBits(coded, bits);
    }

    void writeSE(qint32 value)
    {
        writeUE(value > 0 ? static_cast<quint32>(value) * 2 - 1 : static_cast<quint32>(-value) * 2);
    }

    void writeTrailingBits()
    {
        writeBit(1);
        while (total_bits_ % 8)
            writeBit(0);
    }

    const std::vector<quint8>& bytes() const { return bytes_; }
    quint32 bitLength() const { return total_bits_; }

private:
    void writeBit(quint32 bit)
    {
        if (total_bits_ % 8 == 0)
            bytes_.push_back(0);
        if (bit)
            bytes_.back() |= static_cast<quint8>(1 << (7 - total_bits_ % 8));
        ++total_bits_;
    }

    std::vector<quint8> bytes_;
    quint32 total_bits_ = 0;
};

//--------------------------------------------------------------------------------------------------
void beginNal(BitWriter* writer, int nal_ref_idc, int nal_unit_type)
{
    writer->writeBits(0x00000001, 32); // Annex B start code.
    writer->writeBits(0, 1); // forbidden_zero_bit
    writer->writeBits(static_cast<quint32>(nal_ref_idc), 2);
    writer->writeBits(static_cast<quint32>(nal_unit_type), 5);
}

//--------------------------------------------------------------------------------------------------
// Adds emulation prevention bytes to the NAL payload. Only complete bytes are examined: a slice
// header ends mid-byte and the driver continues writing slice data from there, so the last partial
// byte must be left untouched.
std::vector<quint8> insertEmulationPrevention(const std::vector<quint8>& in, quint32* bit_length)
{
    const size_t complete_bytes = *bit_length / 8;

    std::vector<quint8> out;
    out.reserve(in.size() + 8);

    int zeros = 0;
    for (size_t i = 0; i < in.size(); ++i)
    {
        const quint8 byte = in[i];

        // The 4-byte start code is not part of the payload and must stay as is.
        if (i >= 4 && i < complete_bytes && zeros >= 2 && byte <= 3)
        {
            out.push_back(3);
            *bit_length += 8;
            zeros = 0;
        }

        out.push_back(byte);
        zeros = (byte == 0) ? zeros + 1 : 0;
    }

    return out;
}

//--------------------------------------------------------------------------------------------------
void fillInvalidPicture(VAPictureH264* picture)
{
    picture->picture_id = VA_INVALID_SURFACE;
    picture->frame_idx = 0;
    picture->flags = VA_PICTURE_H264_INVALID;
    picture->TopFieldOrderCnt = 0;
    picture->BottomFieldOrderCnt = 0;
}

//--------------------------------------------------------------------------------------------------
int levelForMacroblocks(quint32 frame_mbs)
{
    for (const H264LevelLimit& limit : kLevelLimits)
    {
        if (frame_mbs <= limit.max_frame_mbs &&
            frame_mbs * static_cast<quint32>(kFrameRate) <= limit.max_mbs_per_sec)
        {
            return limit.level_idc;
        }
    }
    return kLevelLimits[sizeof(kLevelLimits) / sizeof(kLevelLimits[0]) - 1].level_idc;
}

//--------------------------------------------------------------------------------------------------
int roundToTwosMultiple(int x)
{
    return x & (~1);
}

//--------------------------------------------------------------------------------------------------
// Aligns rect to even coordinates: 4:2:0 chroma is subsampled 2x2, so odd edges would bleed color
// from the neighboring pixels.
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
// static
std::unique_ptr<VideoEncoderH264VA> VideoEncoderH264VA::create()
{
    // The actual codec is configured lazily once the frame size is known.
    std::unique_ptr<VideoEncoderH264VA> encoder(new VideoEncoderH264VA());
    if (!encoder->openDisplay())
    {
        LOG(ERROR) << "No VAAPI H264 encoder available";
        return nullptr;
    }
    return encoder;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VA::~VideoEncoderH264VA()
{
    destroyEncoder();
    closeDisplay();
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoEncoderH264VA::isHardwareSupported()
{
    VideoEncoderH264VA probe;
    return probe.openDisplay();
}

//--------------------------------------------------------------------------------------------------
VideoEncoder::Result VideoEncoderH264VA::encode(const Frame* frame, proto::video::Packet* packet)
{
    if (!frame || !packet)
        return Result::PERMANENT_ERROR;

    packet->set_encoding(proto::video::ENCODING_H264);

    bool is_key_frame = isKeyFrameRequired();

    if (context_ == VA_INVALID_ID || last_size_ != frame->size())
    {
        const QSize new_size = frame->size();

        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(new_size.width());
        video_rect->set_height(new_size.height());

        if (!createEncoder(new_size))
        {
            // The hardware encoder refuses frame sizes outside its supported profile/level. This is
            // the signal for the caller to fall back to a software codec.
            LOG(ERROR) << "Unable to create H264 encoder for" << new_size;
            return Result::PERMANENT_ERROR;
        }

        last_size_ = new_size;
        is_key_frame = true;
    }

    const QRect image_rect(QPoint(0, 0), last_size_);

    if (is_key_frame)
    {
        proto::video::Rect* dirty_rect = packet->add_dirty_rect();
        dirty_rect->set_x(0);
        dirty_rect->set_y(0);
        dirty_rect->set_width(last_size_.width());
        dirty_rect->set_height(last_size_.height());

        frame_num_ = 0;
        idr_pic_id_ = (idr_pic_id_ + 1) & 0xFFFF;
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

    if (!uploadFrame(frame))
        return Result::TEMPORARY_ERROR;

    if (!renderFrame(is_key_frame) || !readCodedBuffer())
    {
        // The reconstructed reference may be inconsistent after a failed submission; restart from
        // an IDR frame so the stream recovers cleanly.
        setKeyFrameRequired(true);
        return Result::TEMPORARY_ERROR;
    }

    packet->set_data(std::move(encode_buffer_));
    if (is_key_frame)
        packet->set_flags(proto::video::PACKET_FLAG_IS_KEY_FRAME);

    frame_num_ = (frame_num_ + 1) & ((1u << kLog2MaxFrameNum) - 1);
    current_recon_ ^= 1;
    setKeyFrameRequired(false);
    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VA::setBandwidth(qint64 bandwidth)
{
    if (bandwidth <= 0)
    {
        // Bandwidth is not measured yet - fall back to the defaults baked into the header.
        min_quantizer_ = 16;
        max_quantizer_ = 28;
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
        if (bandwidth < 70 * 1024) // < 70 KB/s
        {
            min_quantizer_ = 24;
            max_quantizer_ = 46;
        }
        else if (bandwidth < 150 * 1024) // < 150 KB/s
        {
            min_quantizer_ = 22;
            max_quantizer_ = 42;
        }
        else if (bandwidth < 500 * 1024) // < 500 KB/s
        {
            min_quantizer_ = 20;
            max_quantizer_ = 38;
        }
        else if (bandwidth < 2 * 1024 * 1024) // < 2 MB/s
        {
            min_quantizer_ = 18;
            max_quantizer_ = 32;
        }
        else
        {
            min_quantizer_ = 14;
            max_quantizer_ = 26;
        }
    }

    // In CQP mode the quality is driven by the slice QP alone; it is applied through
    // slice_qp_delta, so no encoder re-creation (and thus no spurious key frame) is needed.
    cqp_qp_ = std::clamp((min_quantizer_ + max_quantizer_) / 2, 1u, 51u);

    // The updated rate control parameters are sent to the driver with the next frame.
    rc_dirty_ = true;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VA::VideoEncoderH264VA()
    : VideoEncoder(proto::video::ENCODING_H264)
{
    LOG(INFO) << "Ctor";
    upload_image_.image_id = VA_INVALID_ID;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::openDisplay()
{
    if (display_)
        return true;

    if (!loadLibva())
    {
        LOG(WARNING) << "libva is not available";
        return false;
    }

    for (int node = kFirstRenderNode; node < kFirstRenderNode + kRenderNodeCount; ++node)
    {
        char path[32];
        snprintf(path, sizeof(path), "/dev/dri/renderD%d", node);

        const int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0)
            continue;

        VADisplay display = VA_DRM(vaGetDisplayDRM)(fd);
        if (!display)
        {
            close(fd);
            continue;
        }

        VA(vaSetErrorCallback)(display, vaMessageToLog, nullptr);
        VA(vaSetInfoCallback)(display, vaMessageToLog, nullptr);

        int major = 0;
        int minor = 0;
        if (VA(vaInitialize)(display, &major, &minor) != VA_STATUS_SUCCESS)
        {
            VA(vaTerminate)(display);
            close(fd);
            continue;
        }

        display_ = display;
        drm_fd_ = fd;

        if (selectConfig())
        {
            LOG(INFO) << "VAAPI H264 encoder on" << path << ":" << VA(vaQueryVendorString)(display_)
                      << "profile:" << profile_ << "entrypoint:" << entrypoint_
                      << "rc:" << rc_mode_ << "packed headers:" << packed_headers_;
            return true;
        }

        closeDisplay();
    }

    LOG(INFO) << "No VAAPI H264 encode support found";
    return false;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VA::closeDisplay()
{
    if (display_)
    {
        VA(vaTerminate)(display_);
        display_ = nullptr;
    }

    if (drm_fd_ != -1)
    {
        close(drm_fd_);
        drm_fd_ = -1;
    }
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::selectConfig()
{
    std::vector<VAProfile> profiles(static_cast<size_t>(VA(vaMaxNumProfiles)(display_)));
    int num_profiles = 0;
    if (VA(vaQueryConfigProfiles)(display_, profiles.data(), &num_profiles) != VA_STATUS_SUCCESS)
        return false;
    profiles.resize(static_cast<size_t>(num_profiles));

    // Higher profiles compress better (CABAC); constrained baseline is the last resort.
    const VAProfile kCandidates[] =
    {
        VAProfileH264High,
        VAProfileH264Main,
        VAProfileH264ConstrainedBaseline
    };

    for (VAProfile candidate : kCandidates)
    {
        if (std::find(profiles.begin(), profiles.end(), candidate) == profiles.end())
            continue;

        std::vector<VAEntrypoint> entrypoints(static_cast<size_t>(VA(vaMaxNumEntrypoints)(display_)));
        int num_entrypoints = 0;
        if (VA(vaQueryConfigEntrypoints)(display_, candidate, entrypoints.data(),
                                         &num_entrypoints) != VA_STATUS_SUCCESS)
        {
            continue;
        }
        entrypoints.resize(static_cast<size_t>(num_entrypoints));

        // The full-feature entrypoint first; the low-power one is the only choice on newer Intel
        // GPUs.
        for (VAEntrypoint entry : { VAEntrypointEncSlice, VAEntrypointEncSliceLP })
        {
            if (std::find(entrypoints.begin(), entrypoints.end(), entry) == entrypoints.end())
                continue;

            VAConfigAttrib attribs[3] = {};
            attribs[0].type = VAConfigAttribRTFormat;
            attribs[1].type = VAConfigAttribRateControl;
            attribs[2].type = VAConfigAttribEncPackedHeaders;
            if (VA(vaGetConfigAttributes)(display_, candidate, entry, attribs, 3) != VA_STATUS_SUCCESS)
                continue;

            if (attribs[0].value == VA_ATTRIB_NOT_SUPPORTED || !(attribs[0].value & VA_RT_FORMAT_YUV420))
                continue;

            const quint32 rc_modes = attribs[1].value != VA_ATTRIB_NOT_SUPPORTED ? attribs[1].value : 0;
            quint32 rc_mode = 0;
            if (rc_modes & VA_RC_VBR)
                rc_mode = VA_RC_VBR;
            else if (rc_modes & VA_RC_CBR)
                rc_mode = VA_RC_CBR;
            else if (rc_modes & VA_RC_CQP)
                rc_mode = VA_RC_CQP;
            else
                continue;

            profile_ = candidate;
            entrypoint_ = entry;
            rc_mode_ = rc_mode;

            // The driver generates the headers it does not list here by itself.
            packed_headers_ = 0;
            if (attribs[2].value != VA_ATTRIB_NOT_SUPPORTED)
            {
                packed_headers_ = attribs[2].value & (VA_ENC_PACKED_HEADER_SEQUENCE |
                                                      VA_ENC_PACKED_HEADER_PICTURE |
                                                      VA_ENC_PACKED_HEADER_SLICE);
            }
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::createEncoder(const QSize& size)
{
    destroyEncoder();

    // 4:2:0 frame cropping cannot represent odd sizes; such frames go to the software codecs.
    if (size.isEmpty() || (size.width() & 1) || (size.height() & 1))
    {
        LOG(ERROR) << "Unsupported frame size for VAAPI H264 encoder:" << size;
        return false;
    }

    mb_width_ = (size.width() + kMacroblockSize - 1) / kMacroblockSize;
    mb_height_ = (size.height() + kMacroblockSize - 1) / kMacroblockSize;
    level_idc_ = levelForMacroblocks(static_cast<quint32>(mb_width_ * mb_height_));
    cabac_ = profile_ != VAProfileH264ConstrainedBaseline;

    if (rc_mode_ == VA_RC_CQP)
        pic_init_qp_ = cqp_qp_;
    else
        pic_init_qp_ = 26;

    VAConfigAttrib attribs[3];
    int attrib_count = 0;
    attribs[attrib_count].type = VAConfigAttribRTFormat;
    attribs[attrib_count].value = VA_RT_FORMAT_YUV420;
    ++attrib_count;
    attribs[attrib_count].type = VAConfigAttribRateControl;
    attribs[attrib_count].value = rc_mode_;
    ++attrib_count;
    if (packed_headers_)
    {
        attribs[attrib_count].type = VAConfigAttribEncPackedHeaders;
        attribs[attrib_count].value = packed_headers_;
        ++attrib_count;
    }

    VAStatus status = VA(vaCreateConfig)(display_, profile_, entrypoint_, attribs, attrib_count,
                                         &config_);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaCreateConfig failed:" << VA(vaErrorStr)(status);
        destroyEncoder();
        return false;
    }

    const quint32 aligned_width = static_cast<quint32>(mb_width_ * kMacroblockSize);
    const quint32 aligned_height = static_cast<quint32>(mb_height_ * kMacroblockSize);

    VASurfaceAttrib surface_attrib = {};
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = VA_FOURCC_NV12;

    VASurfaceID surfaces[3] = { VA_INVALID_SURFACE, VA_INVALID_SURFACE, VA_INVALID_SURFACE };
    status = VA(vaCreateSurfaces)(display_, VA_RT_FORMAT_YUV420, aligned_width, aligned_height,
                                  surfaces, 3, &surface_attrib, 1);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaCreateSurfaces failed:" << VA(vaErrorStr)(status);
        destroyEncoder();
        return false;
    }

    input_surface_ = surfaces[0];
    recon_surfaces_[0] = surfaces[1];
    recon_surfaces_[1] = surfaces[2];

    status = VA(vaCreateContext)(display_, config_, static_cast<int>(aligned_width),
                                 static_cast<int>(aligned_height), VA_PROGRESSIVE, surfaces, 3,
                                 &context_);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaCreateContext failed:" << VA(vaErrorStr)(status);
        destroyEncoder();
        return false;
    }

    // Compressed output never exceeds the raw NV12 size; a little headroom covers the headers.
    const size_t coded_size = static_cast<size_t>(aligned_width) * aligned_height * 3 / 2 + 65536;
    status = VA(vaCreateBuffer)(display_, context_, VAEncCodedBufferType,
                                static_cast<unsigned int>(coded_size), 1, nullptr, &coded_buffer_);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaCreateBuffer failed for coded buffer:" << VA(vaErrorStr)(status);
        destroyEncoder();
        return false;
    }

    current_recon_ = 0;
    frame_num_ = 0;
    rc_dirty_ = true;
    use_derive_ = true;
    first_upload_ = true;

    LOG(INFO) << "VAAPI H264 encoder created for" << size << "level:" << level_idc_
              << (cabac_ ? "CABAC" : "CAVLC");
    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VA::destroyEncoder()
{
    if (!display_)
        return;

    if (context_ != VA_INVALID_ID)
    {
        VA(vaDestroyContext)(display_, context_);
        context_ = VA_INVALID_ID;
    }

    if (coded_buffer_ != VA_INVALID_ID)
    {
        VA(vaDestroyBuffer)(display_, coded_buffer_);
        coded_buffer_ = VA_INVALID_ID;
    }

    if (upload_image_.image_id != VA_INVALID_ID)
    {
        VA(vaDestroyImage)(display_, upload_image_.image_id);
        upload_image_.image_id = VA_INVALID_ID;
    }

    if (input_surface_ != VA_INVALID_SURFACE)
    {
        VASurfaceID surfaces[3] = { input_surface_, recon_surfaces_[0], recon_surfaces_[1] };
        VA(vaDestroySurfaces)(display_, surfaces, 3);
        input_surface_ = VA_INVALID_SURFACE;
        recon_surfaces_[0] = VA_INVALID_SURFACE;
        recon_surfaces_[1] = VA_INVALID_SURFACE;
    }

    if (config_ != VA_INVALID_ID)
    {
        VA(vaDestroyConfig)(display_, config_);
        config_ = VA_INVALID_ID;
    }
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::uploadFrame(const Frame* frame)
{
    if (use_derive_)
    {
        VAImage image;
        if (VA(vaDeriveImage)(display_, input_surface_, &image) == VA_STATUS_SUCCESS)
        {
            if (image.format.fourcc == VA_FOURCC_NV12)
            {
                const bool result = convertIntoImage(frame, &image);
                VA(vaDestroyImage)(display_, image.image_id);
                return result;
            }
            VA(vaDestroyImage)(display_, image.image_id);
        }

        use_derive_ = false;
        first_upload_ = true;
    }

    if (upload_image_.image_id == VA_INVALID_ID)
    {
        VAImageFormat format = {};
        format.fourcc = VA_FOURCC_NV12;
        format.byte_order = VA_LSB_FIRST;
        format.bits_per_pixel = 12;

        const VAStatus status = VA(vaCreateImage)(display_, &format, mb_width_ * kMacroblockSize,
                                                  mb_height_ * kMacroblockSize, &upload_image_);
        if (status != VA_STATUS_SUCCESS)
        {
            LOG(ERROR) << "vaCreateImage failed:" << VA(vaErrorStr)(status);
            upload_image_.image_id = VA_INVALID_ID;
            return false;
        }
    }

    if (!convertIntoImage(frame, &upload_image_))
        return false;

    const VAStatus status = VA(vaPutImage)(display_, input_surface_, upload_image_.image_id,
                                           0, 0, upload_image_.width, upload_image_.height,
                                           0, 0, upload_image_.width, upload_image_.height);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaPutImage failed:" << VA(vaErrorStr)(status);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::convertIntoImage(const Frame* frame, const VAImage* image)
{
    quint8* data = nullptr;
    VAStatus status = VA(vaMapBuffer)(display_, image->buf, reinterpret_cast<void**>(&data));
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaMapBuffer failed:" << VA(vaErrorStr)(status);
        return false;
    }

    if (first_upload_)
    {
        // The surface is macroblock-aligned and the driver memory arrives uninitialized; fill the
        // padding once with neutral grey so it compresses to nothing.
        memset(data, 0x80, image->data_size);
        first_upload_ = false;
    }

    // Frame's BGRA memory layout is what libyuv calls ARGB (little-endian naming).
    libyuv::ARGBToNV12(frame->frameData(), frame->stride(),
                       data + image->offsets[0], static_cast<int>(image->pitches[0]),
                       data + image->offsets[1], static_cast<int>(image->pitches[1]),
                       last_size_.width(), last_size_.height());

    VA(vaUnmapBuffer)(display_, image->buf);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::renderFrame(bool is_idr)
{
    std::vector<VABufferID> buffers;

    auto create_buffer = [&](VABufferType type, const void* data, size_t size)
    {
        VABufferID buffer = VA_INVALID_ID;
        const VAStatus status = VA(vaCreateBuffer)(display_, context_, type,
                                                   static_cast<unsigned int>(size), 1,
                                                   const_cast<void*>(data), &buffer);
        if (status != VA_STATUS_SUCCESS)
        {
            LOG(ERROR) << "vaCreateBuffer failed:" << VA(vaErrorStr)(status);
            return false;
        }
        buffers.push_back(buffer);
        return true;
    };

    auto create_misc_buffer = [&](VAEncMiscParameterType type, const void* payload, size_t size)
    {
        std::vector<quint8> blob(sizeof(VAEncMiscParameterBuffer) + size);
        VAEncMiscParameterBuffer* misc = reinterpret_cast<VAEncMiscParameterBuffer*>(blob.data());
        misc->type = type;
        memcpy(misc->data, payload, size);
        return create_buffer(VAEncMiscParameterBufferType, blob.data(), blob.size());
    };

    auto create_packed_header = [&](VAEncPackedHeaderType type, const PackedHeader& header)
    {
        VAEncPackedHeaderParameterBuffer param = {};
        param.type = type;
        param.bit_length = header.bit_length;
        param.has_emulation_bytes = 1;
        return create_buffer(VAEncPackedHeaderParameterBufferType, &param, sizeof(param)) &&
               create_buffer(VAEncPackedHeaderDataBufferType, header.data.data(), header.data.size());
    };

    const quint32 max_frame_num = 1u << kLog2MaxFrameNum;
    const bool need_rc = rc_mode_ != VA_RC_CQP && (is_idr || rc_dirty_);
    bool ok = true;

    if (is_idr)
    {
        VAEncSequenceParameterBufferH264 seq = {};
        seq.seq_parameter_set_id = 0;
        seq.level_idc = static_cast<quint8>(level_idc_);
        seq.intra_period = kIFrameInterval;
        seq.intra_idr_period = kIFrameInterval;
        seq.ip_period = 1;
        seq.bits_per_second = maxRateBps();
        seq.max_num_ref_frames = 1;
        seq.picture_width_in_mbs = static_cast<quint16>(mb_width_);
        seq.picture_height_in_mbs = static_cast<quint16>(mb_height_);
        seq.seq_fields.bits.chroma_format_idc = 1;
        seq.seq_fields.bits.frame_mbs_only_flag = 1;
        seq.seq_fields.bits.direct_8x8_inference_flag = 1;
        seq.seq_fields.bits.log2_max_frame_num_minus4 = kLog2MaxFrameNum - 4;
        seq.seq_fields.bits.pic_order_cnt_type = 2;

        const int crop_right = mb_width_ * kMacroblockSize - last_size_.width();
        const int crop_bottom = mb_height_ * kMacroblockSize - last_size_.height();
        if (crop_right || crop_bottom)
        {
            seq.frame_cropping_flag = 1;
            seq.frame_crop_right_offset = static_cast<quint32>(crop_right / 2);
            seq.frame_crop_bottom_offset = static_cast<quint32>(crop_bottom / 2);
        }

        ok = create_buffer(VAEncSequenceParameterBufferType, &seq, sizeof(seq));
    }

    if (ok && need_rc)
    {
        VAEncMiscParameterRateControl rc = {};
        rc.bits_per_second = maxRateBps();
        rc.target_percentage = rc_mode_ == VA_RC_CBR ? 100 : kVbrTargetPercentage;
        rc.window_size = kRateControlWindowMs;
        rc.min_qp = min_quantizer_;
        rc.max_qp = max_quantizer_;
        rc.rc_flags.bits.disable_frame_skip = 1;

        VAEncMiscParameterFrameRate frame_rate = {};
        frame_rate.framerate = static_cast<quint32>(kFrameRate);

        ok = create_misc_buffer(VAEncMiscParameterTypeRateControl, &rc, sizeof(rc)) &&
             create_misc_buffer(VAEncMiscParameterTypeFrameRate, &frame_rate, sizeof(frame_rate));
    }

    if (ok && is_idr && (packed_headers_ & VA_ENC_PACKED_HEADER_SEQUENCE))
        ok = create_packed_header(VAEncPackedHeaderSequence, buildSps());
    if (ok && is_idr && (packed_headers_ & VA_ENC_PACKED_HEADER_PICTURE))
        ok = create_packed_header(VAEncPackedHeaderPicture, buildPps());

    VAEncPictureParameterBufferH264 pic = {};
    if (ok)
    {
        for (VAPictureH264& reference : pic.ReferenceFrames)
            fillInvalidPicture(&reference);

        pic.CurrPic.picture_id = recon_surfaces_[current_recon_];
        pic.CurrPic.frame_idx = frame_num_;
        pic.CurrPic.flags = 0;
        pic.CurrPic.TopFieldOrderCnt = static_cast<qint32>(2 * frame_num_);
        pic.CurrPic.BottomFieldOrderCnt = pic.CurrPic.TopFieldOrderCnt;

        if (!is_idr)
        {
            const quint32 ref_frame_num = (frame_num_ + max_frame_num - 1) % max_frame_num;

            VAPictureH264* reference = &pic.ReferenceFrames[0];
            reference->picture_id = recon_surfaces_[current_recon_ ^ 1];
            reference->frame_idx = ref_frame_num;
            reference->flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
            reference->TopFieldOrderCnt = static_cast<qint32>(2 * ref_frame_num);
            reference->BottomFieldOrderCnt = reference->TopFieldOrderCnt;
        }

        pic.coded_buf = coded_buffer_;
        pic.pic_parameter_set_id = 0;
        pic.seq_parameter_set_id = 0;
        pic.frame_num = static_cast<quint16>(frame_num_);
        pic.pic_init_qp = static_cast<quint8>(pic_init_qp_);
        pic.num_ref_idx_l0_active_minus1 = 0;
        pic.pic_fields.bits.idr_pic_flag = is_idr;
        pic.pic_fields.bits.reference_pic_flag = 1;
        pic.pic_fields.bits.entropy_coding_mode_flag = cabac_;
        pic.pic_fields.bits.deblocking_filter_control_present_flag = 1;

        ok = create_buffer(VAEncPictureParameterBufferType, &pic, sizeof(pic));
    }

    if (ok && (packed_headers_ & VA_ENC_PACKED_HEADER_SLICE))
        ok = create_packed_header(VAEncPackedHeaderSlice, buildSliceHeader(is_idr));

    if (ok)
    {
        VAEncSliceParameterBufferH264 slice = {};
        for (VAPictureH264& reference : slice.RefPicList0)
            fillInvalidPicture(&reference);
        for (VAPictureH264& reference : slice.RefPicList1)
            fillInvalidPicture(&reference);

        slice.macroblock_address = 0;
        slice.num_macroblocks = static_cast<quint32>(mb_width_ * mb_height_);
        slice.macroblock_info = VA_INVALID_ID;
        slice.slice_type = is_idr ? 2 : 0;
        slice.pic_parameter_set_id = 0;
        slice.idr_pic_id = static_cast<quint16>(idr_pic_id_);
        slice.num_ref_idx_active_override_flag = 0;
        slice.num_ref_idx_l0_active_minus1 = 0;
        slice.cabac_init_idc = 0;
        slice.slice_qp_delta = static_cast<qint8>(sliceQpDelta());
        slice.disable_deblocking_filter_idc = 0;

        if (!is_idr)
            slice.RefPicList0[0] = pic.ReferenceFrames[0];

        ok = create_buffer(VAEncSliceParameterBufferType, &slice, sizeof(slice));
    }

    if (ok)
    {
        VAStatus status = VA(vaBeginPicture)(display_, context_, input_surface_);
        if (status != VA_STATUS_SUCCESS)
        {
            LOG(ERROR) << "vaBeginPicture failed:" << VA(vaErrorStr)(status);
            ok = false;
        }
        else
        {
            status = VA(vaRenderPicture)(display_, context_, buffers.data(),
                                         static_cast<int>(buffers.size()));
            if (status != VA_STATUS_SUCCESS)
            {
                LOG(ERROR) << "vaRenderPicture failed:" << VA(vaErrorStr)(status);
                ok = false;
            }

            status = VA(vaEndPicture)(display_, context_);
            if (status != VA_STATUS_SUCCESS)
            {
                LOG(ERROR) << "vaEndPicture failed:" << VA(vaErrorStr)(status);
                ok = false;
            }
        }
    }

    for (VABufferID buffer : buffers)
        VA(vaDestroyBuffer)(display_, buffer);

    if (!ok)
        return false;

    const VAStatus status = VA(vaSyncSurface)(display_, input_surface_);
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaSyncSurface failed:" << VA(vaErrorStr)(status);
        return false;
    }

    if (need_rc)
        rc_dirty_ = false;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VA::readCodedBuffer()
{
    VACodedBufferSegment* segment = nullptr;
    const VAStatus status = VA(vaMapBuffer)(display_, coded_buffer_,
                                            reinterpret_cast<void**>(&segment));
    if (status != VA_STATUS_SUCCESS)
    {
        LOG(ERROR) << "vaMapBuffer failed for coded buffer:" << VA(vaErrorStr)(status);
        return false;
    }

    bool ok = true;
    encode_buffer_.clear();

    for (; segment; segment = reinterpret_cast<VACodedBufferSegment*>(segment->next))
    {
        if (segment->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK)
        {
            LOG(ERROR) << "Coded buffer overflow";
            ok = false;
            break;
        }

        encode_buffer_.append(static_cast<const char*>(segment->buf), segment->size);
    }

    VA(vaUnmapBuffer)(display_, coded_buffer_);
    return ok && !encode_buffer_.empty();
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VA::PackedHeader VideoEncoderH264VA::buildSps() const
{
    int profile_idc;
    switch (profile_)
    {
        case VAProfileH264High:
            profile_idc = 100;
            break;
        case VAProfileH264Main:
            profile_idc = 77;
            break;
        default:
            profile_idc = 66;
            break;
    }

    BitWriter bs;
    beginNal(&bs, 3, 7);

    bs.writeBits(static_cast<quint32>(profile_idc), 8);
    // constraint_set0/1 mark the constrained baseline subset; reserved bits are zero.
    bs.writeBits(profile_idc == 66 ? 0xC0 : 0x00, 8);
    bs.writeBits(static_cast<quint32>(level_idc_), 8);
    bs.writeUE(0); // seq_parameter_set_id

    if (profile_idc == 100)
    {
        bs.writeUE(1); // chroma_format_idc: 4:2:0
        bs.writeUE(0); // bit_depth_luma_minus8
        bs.writeUE(0); // bit_depth_chroma_minus8
        bs.writeBits(0, 1); // qpprime_y_zero_transform_bypass_flag
        bs.writeBits(0, 1); // seq_scaling_matrix_present_flag
    }

    bs.writeUE(kLog2MaxFrameNum - 4);
    bs.writeUE(2); // pic_order_cnt_type: output order equals coding order (no B frames)
    bs.writeUE(1); // max_num_ref_frames
    bs.writeBits(0, 1); // gaps_in_frame_num_value_allowed_flag
    bs.writeUE(static_cast<quint32>(mb_width_ - 1));
    bs.writeUE(static_cast<quint32>(mb_height_ - 1));
    bs.writeBits(1, 1); // frame_mbs_only_flag
    bs.writeBits(1, 1); // direct_8x8_inference_flag

    const int crop_right = mb_width_ * kMacroblockSize - last_size_.width();
    const int crop_bottom = mb_height_ * kMacroblockSize - last_size_.height();
    if (crop_right || crop_bottom)
    {
        bs.writeBits(1, 1); // frame_cropping_flag
        bs.writeUE(0);
        bs.writeUE(static_cast<quint32>(crop_right / 2));
        bs.writeUE(0);
        bs.writeUE(static_cast<quint32>(crop_bottom / 2));
    }
    else
    {
        bs.writeBits(0, 1);
    }

    bs.writeBits(0, 1); // vui_parameters_present_flag
    bs.writeTrailingBits();

    PackedHeader header;
    header.bit_length = bs.bitLength();
    header.data = insertEmulationPrevention(bs.bytes(), &header.bit_length);
    return header;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VA::PackedHeader VideoEncoderH264VA::buildPps() const
{
    BitWriter bs;
    beginNal(&bs, 3, 8);

    bs.writeUE(0); // pic_parameter_set_id
    bs.writeUE(0); // seq_parameter_set_id
    bs.writeBits(cabac_ ? 1 : 0, 1); // entropy_coding_mode_flag
    bs.writeBits(0, 1); // bottom_field_pic_order_in_frame_present_flag
    bs.writeUE(0); // num_slice_groups_minus1
    bs.writeUE(0); // num_ref_idx_l0_default_active_minus1
    bs.writeUE(0); // num_ref_idx_l1_default_active_minus1
    bs.writeBits(0, 1); // weighted_pred_flag
    bs.writeBits(0, 2); // weighted_bipred_idc
    bs.writeSE(static_cast<qint32>(pic_init_qp_) - 26); // pic_init_qp_minus26
    bs.writeSE(0); // pic_init_qs_minus26
    bs.writeSE(0); // chroma_qp_index_offset
    bs.writeBits(1, 1); // deblocking_filter_control_present_flag
    bs.writeBits(0, 1); // constrained_intra_pred_flag
    bs.writeBits(0, 1); // redundant_pic_cnt_present_flag
    bs.writeTrailingBits();

    PackedHeader header;
    header.bit_length = bs.bitLength();
    header.data = insertEmulationPrevention(bs.bytes(), &header.bit_length);
    return header;
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VA::PackedHeader VideoEncoderH264VA::buildSliceHeader(bool is_idr) const
{
    BitWriter bs;
    beginNal(&bs, is_idr ? 3 : 2, is_idr ? 5 : 1);

    bs.writeUE(0); // first_mb_in_slice
    bs.writeUE(is_idr ? 2 : 0); // slice_type: I or P
    bs.writeUE(0); // pic_parameter_set_id
    bs.writeBits(frame_num_, static_cast<int>(kLog2MaxFrameNum));

    if (is_idr)
    {
        bs.writeUE(idr_pic_id_);

        // dec_ref_pic_marking
        bs.writeBits(0, 1); // no_output_of_prior_pics_flag
        bs.writeBits(0, 1); // long_term_reference_flag
    }
    else
    {
        bs.writeBits(0, 1); // num_ref_idx_active_override_flag
        bs.writeBits(0, 1); // ref_pic_list_modification_flag_l0

        // dec_ref_pic_marking
        bs.writeBits(0, 1); // adaptive_ref_pic_marking_mode_flag

        if (cabac_)
            bs.writeUE(0); // cabac_init_idc
    }

    bs.writeSE(sliceQpDelta());
    bs.writeUE(0); // disable_deblocking_filter_idc
    bs.writeSE(0); // slice_alpha_c0_offset_div2
    bs.writeSE(0); // slice_beta_offset_div2

    // No trailing bits: the driver continues the slice data right after the header.
    PackedHeader header;
    header.bit_length = bs.bitLength();
    header.data = insertEmulationPrevention(bs.bytes(), &header.bit_length);
    return header;
}

//--------------------------------------------------------------------------------------------------
// The peak bitrate handed to the driver; the target is derived from it via target_percentage.
quint32 VideoEncoderH264VA::maxRateBps() const
{
    if (rc_mode_ == VA_RC_CQP)
        return 0;
    if (rc_mode_ == VA_RC_CBR)
        return target_bitrate_bps_;
    return static_cast<quint32>(static_cast<quint64>(target_bitrate_bps_) * 100 /
                                kVbrTargetPercentage);
}

//--------------------------------------------------------------------------------------------------
// In CQP mode bandwidth changes shift the slice QP relative to the QP baked into the PPS.
qint32 VideoEncoderH264VA::sliceQpDelta() const
{
    if (rc_mode_ != VA_RC_CQP)
        return 0;
    return static_cast<qint32>(cqp_qp_) - static_cast<qint32>(pic_init_qp_);
}
