//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/pixel_translator_argb.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_ARGB_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_ARGB_H

#include "aspia_config.h"

#include "base/macros.h"
#include "codec/pixel_translator.h"

namespace aspia {

//
// Convertation from 32bpp 0RGB (alpha channel = 0) to user defined
//

template<const int bytes_per_pixel>
class PixelTranslatorARGB : public PixelTranslator
{
public:
    PixelTranslatorARGB(const PixelFormat& format)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            red_table_[i]   = ((i * format.RedMax()   + 255 / 2) / 255) << format.RedShift();
            green_table_[i] = ((i * format.GreenMax() + 255 / 2) / 255) << format.GreenShift();
            blue_table_[i]  = ((i * format.BlueMax()  + 255 / 2) / 255) << format.BlueShift();
        }
    }

    ~PixelTranslatorARGB() = default;

    void Translate(const uint8_t* src,
                   int src_stride,
                   uint8_t* dst,
                   int dst_stride,
                   int width,
                   int height) override
    { 
        src_stride -= width * sizeof(uint32_t);
        dst_stride -= width * bytes_per_pixel;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t red   = red_table_[*(uint32_t*)src >> 16 & 255];
                uint32_t green = green_table_[*(uint32_t*)src >> 8 & 255];
                uint32_t blue  = blue_table_[*(uint32_t*)src & 255];

                if (bytes_per_pixel == 4)
                {
                    *(uint32_t*)dst = (uint32_t)(red | green | blue);
                }
                else if (bytes_per_pixel == 3)
                {
                    dst[0] = blue;
                    dst[1] = green >> 8;
                    dst[2] = red >> 16;
                }
                else if (bytes_per_pixel == 2)
                {
                    *(uint16_t*)dst = (uint16_t)(red | green | blue);
                }
                else if (bytes_per_pixel == 1)
                {
                    *(uint8_t*)dst = (uint8_t)(red | green | blue);
                }

                src += sizeof(uint32_t);
                dst += bytes_per_pixel;
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    uint32_t red_table_[256];
    uint32_t green_table_[256];
    uint32_t blue_table_[256];

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorARGB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_ARGB_H
