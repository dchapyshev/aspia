/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_fromRGB565.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H
#define _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H

#include "aspia_config.h"

#include <vector>

#include "base/macros.h"
#include "codec/pixel_translator.h"

namespace aspia {

//
// Convertation from 16bpp RGB565 to user defined
//

template<typename T>
class PixelTranslatorFromRGB565 : public PixelTranslator
{
public:
    PixelTranslatorFromRGB565(const PixelFormat &src_format, const PixelFormat &dst_format)
    {
        InitTable(src_format, dst_format);
    }

    ~PixelTranslatorFromRGB565()
    {
        // Nothing
    }

    void Translate(const uint8_t *src,
                   int src_stride,
                   uint8_t *dst,
                   int dst_stride,
                   int width,
                   int height) override
    {
        src_stride -= width * sizeof(uint16_t);
        dst_stride -= width * sizeof(T);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                *(T*)dst = (T)(table_[*(uint16_t*)src]);

                src += sizeof(uint16_t);
                dst += sizeof(T);
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    void InitTable(const PixelFormat &src_format, const PixelFormat &dst_format)
    {
        table_.resize(kTableSize);

        uint32_t src_red_mask   = src_format.RedMax()   << src_format.RedShift();
        uint32_t src_green_mask = src_format.GreenMax() << src_format.GreenShift();
        uint32_t src_blue_mask  = src_format.BlueMax()  << src_format.BlueShift();

        for (int i = 0; i < kTableSize; ++i)
        {
            uint32_t src_red   = (i & src_red_mask)   >> src_format.RedShift();
            uint32_t src_green = (i & src_green_mask) >> src_format.GreenShift();
            uint32_t src_blue  = (i & src_blue_mask)  >> src_format.BlueShift();

            uint32_t dst_red   = (src_red   * dst_format.RedMax()   / src_format.RedMax())   << dst_format.RedShift();
            uint32_t dst_green = (src_green * dst_format.GreenMax() / src_format.GreenMax()) << dst_format.GreenShift();
            uint32_t dst_blue  = (src_blue  * dst_format.BlueMax()  / src_format.BlueMax())  << dst_format.BlueShift();

            table_[i] = (dst_red | dst_green | dst_blue);
        }
    }

private:
    static const size_t kTableSize = 65536;
    std::vector<uint32_t> table_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFromRGB565);
};

} // namespace aspia

#endif // _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H
