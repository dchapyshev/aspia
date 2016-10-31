/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_fromRGB565.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H
#define _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H

#include "aspia_config.h"

#include <assert.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/logging.h"

#include "codec/pixel_translator.h"

//
// Convertation from 16bpp RGB565 to user defined
//

template<typename T>
class PixelTranslatorFromRGB565 : public PixelTranslator
{
public:
    PixelTranslatorFromRGB565(const PixelFormat &src_format, const PixelFormat &dst_format)
    {
        CHECK(sizeof(T) == dst_format.bytes_per_pixel());
        CHECK(sizeof(uint16_t) == src_format.bytes_per_pixel());

        InitTable(src_format, dst_format);
    }

    ~PixelTranslatorFromRGB565() {}

    virtual void Translate(const uint8_t *src,
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

        uint32_t src_red_mask   = src_format.red_max()   << src_format.red_shift();
        uint32_t src_green_mask = src_format.green_max() << src_format.green_shift();
        uint32_t src_blue_mask  = src_format.blue_max()  << src_format.blue_shift();

        for (int i = 0; i < kTableSize; ++i)
        {
            uint32_t src_red   = (i & src_red_mask)   >> src_format.red_shift();
            uint32_t src_green = (i & src_green_mask) >> src_format.green_shift();
            uint32_t src_blue  = (i & src_blue_mask)  >> src_format.blue_shift();

            uint32_t dst_red   = (src_red   * dst_format.red_max()   / src_format.red_max())   << dst_format.red_shift();
            uint32_t dst_green = (src_green * dst_format.green_max() / src_format.green_max()) << dst_format.green_shift();
            uint32_t dst_blue  = (src_blue  * dst_format.blue_max()  / src_format.blue_max())  << dst_format.blue_shift();

            table_[i] = (dst_red | dst_green | dst_blue);
        }
    }

private:
    static const size_t kTableSize = 65536;
    std::vector<uint32_t> table_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFromRGB565);
};

#endif // _ASPIA_PIXEL_TRANSLATOR_FROMRGB565_H
