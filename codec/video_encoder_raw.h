/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder_raw.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_RAW_H
#define _ASPIA_CODEC__VIDEO_ENCODER_RAW_H

#include "aspia_config.h"

#include <memory>

#include "codec/video_encoder.h"
#include "codec/pixel_translator_selector.h"
#include "base/macros.h"
#include "base/logging.h"

class VideoEncoderRAW : public VideoEncoder
{
public:
    VideoEncoderRAW();
    virtual ~VideoEncoderRAW() override;

    virtual proto::VideoPacket* Encode(const DesktopSize &desktop_size,
                                       const PixelFormat &src_format,
                                       const PixelFormat &dst_format,
                                       const DesktopRegion &changed_region,
                                       const uint8_t *src_buffer) override;

private:
    uint8_t* GetOutputBuffer(proto::VideoPacket *packet, size_t size);

    void PrepareResources(const PixelFormat &src_format,
                          const PixelFormat &dst_format);

private:
    int32_t packet_flags_;

    std::unique_ptr<DesktopRegion::Iterator> rect_iterator;

    PixelFormat current_src_format_;
    PixelFormat current_dst_format_;

    std::unique_ptr<PixelTranslator> translator_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderRAW);
};

#endif // _ASPIA_CODEC__VIDEO_ENCODER_RAW_H
