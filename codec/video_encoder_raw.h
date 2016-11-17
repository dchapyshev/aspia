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

class VideoEncoderRAW : public VideoEncoder
{
public:
    VideoEncoderRAW();
    virtual ~VideoEncoderRAW() override;

    virtual void Resize(const DesktopSize &screen_size,
                        const PixelFormat &host_pixel_format,
                        const PixelFormat &client_pixel_format) override;

    virtual Status Encode(proto::VideoPacket *packet,
                          const uint8_t *screen_buffer,
                          const DesktopRegion &changed_region) override;

private:
    uint8_t* GetOutputBuffer(proto::VideoPacket *packet, size_t size);

private:
    bool size_changed_;

    int32_t packet_flags_;

    std::unique_ptr<DesktopRegion::Iterator> rect_iterator;

    DesktopSize screen_size_;
    PixelFormat client_pixel_format_;

    int src_bytes_per_pixel_;
    int dst_bytes_per_pixel_;
    int src_bytes_per_row_;

    std::unique_ptr<PixelTranslator> translator_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderRAW);
};

#endif // _ASPIA_CODEC__VIDEO_ENCODER_RAW_H
