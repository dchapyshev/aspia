//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/pixel_translator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/pixel_translator.h"

namespace aspia {

PixelTranslator::PixelTranslator(const PixelFormat& format)
{
    dst_bytes_per_pixel_ = format.BytesPerPixel();

    for (uint32_t i = 0; i < 256; ++i)
    {
        red_table_[i] = ((i * format.RedMax()   + 255 / 2) / 255)
            << format.RedShift();

        green_table_[i] = ((i * format.GreenMax() + 255 / 2) / 255)
            << format.GreenShift();

        blue_table_[i] = ((i * format.BlueMax()  + 255 / 2) / 255)
            << format.BlueShift();
    }
}

void PixelTranslator::Translate(const uint8_t* src,
                                int src_stride,
                                uint8_t* dst,
                                int dst_stride,
                                int width,
                                int height)
{
    src_stride -= width * sizeof(uint32_t);
    dst_stride -= width * dst_bytes_per_pixel_;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint32_t red   = red_table_[*(uint32_t*) src >> 16 & 255];
            uint32_t green = green_table_[*(uint32_t*) src >> 8 & 255];
            uint32_t blue  = blue_table_[*(uint32_t*) src & 255];

            switch (dst_bytes_per_pixel_)
            {
                case 1:
                    *(uint8_t*) dst = (uint8_t) (red | green | blue);
                    break;

                case 2:
                    *(uint16_t*) dst = (uint16_t) (red | green | blue);
                    break;

                case 3:
                    dst[0] = blue;
                    dst[1] = green >> 8;
                    dst[2] = red >> 16;
                    break;

                case 4:
                    *(uint32_t*) dst = (uint32_t) (red | green | blue);
                    break;
            }

            src += sizeof(uint32_t);
            dst += dst_bytes_per_pixel_;
        }

        src += src_stride;
        dst += dst_stride;
    }
}

} // namespace aspia
