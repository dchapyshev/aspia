//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/pixel_translator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_H

#include "base/macros.h"
#include "desktop_capture/pixel_format.h"

namespace aspia {

// Convertation from 32bpp ARGB to user defined pixel format.
class PixelTranslator
{
public:
    explicit PixelTranslator(const PixelFormat& format);
    ~PixelTranslator() = default;

    void Translate(const uint8_t* src,
                   int src_stride,
                   uint8_t* dst,
                   int dst_stride,
                   int width,
                   int height);

private:
    uint32_t red_table_[256];
    uint32_t green_table_[256];
    uint32_t blue_table_[256];

    int dst_bytes_per_pixel_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslator);
};

} // namespace aspia

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_H
