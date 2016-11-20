/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_fromARGB.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_PIXEL_TRANSLATOR_ARGB_H
#define _ASPIA_PIXEL_TRANSLATOR_ARGB_H

#include "aspia_config.h"

#include <vector>

#include "base/macros.h"
#include "codec/pixel_translator.h"

//
// Convertation from 32bpp 0RGB (alpha channel = 0) to user defined
//

template<typename T>
class PixelTranslatorFromARGB : public PixelTranslator
{
public:
    PixelTranslatorFromARGB(const PixelFormat &src_format, const PixelFormat &dst_format) :
        src_format_(src_format)
    {
        InitRedTable(dst_format);
        InitGreenTable(dst_format);
        InitBlueTable(dst_format);
    }

    ~PixelTranslatorFromARGB()
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
        src_stride -= width * sizeof(uint32_t);
        dst_stride -= width * sizeof(T);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                uint32_t red   = red_table_[*(uint32_t*)src >> src_format_.red_shift() & src_format_.red_max()];
                uint32_t green = green_table_[*(uint32_t*)src >> src_format_.green_shift() & src_format_.green_max()];
                uint32_t blue  = blue_table_[*(uint32_t*)src >> src_format_.blue_shift() & src_format_.blue_max()];

                *(T*)dst = (T)(red | green | blue);

                src += sizeof(uint32_t);
                dst += sizeof(T);
            }

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    void InitRedTable(const PixelFormat &dst_format)
    {
        red_table_.resize(src_format_.red_max() + 1);

        for (uint32_t i = 0; i <= src_format_.red_max(); ++i)
        {
            red_table_[i] = ((i * dst_format.red_max() + src_format_.red_max() / 2) / src_format_.red_max()) << dst_format.red_shift();
        }
    }

    void InitGreenTable(const PixelFormat &dst_format)
    {
        green_table_.resize(src_format_.green_max() + 1);

        for (uint32_t i = 0; i <= src_format_.green_max(); ++i)
        {
            green_table_[i] = ((i * dst_format.green_max() + src_format_.green_max() / 2) / src_format_.green_max()) << dst_format.green_shift();
        }
    }

    void InitBlueTable(const PixelFormat &dst_format)
    {
        blue_table_.resize(src_format_.blue_max() + 1);

        for (uint32_t i = 0; i <= src_format_.blue_max(); ++i)
        {
            blue_table_[i] = ((i * dst_format.blue_max() + src_format_.blue_max() / 2) / src_format_.blue_max()) << dst_format.blue_shift();
        }
    }

private:
    PixelFormat src_format_;

    std::vector<uint32_t> red_table_;
    std::vector<uint32_t> green_table_;
    std::vector<uint32_t> blue_table_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFromARGB);
};

#endif // _ASPIA_PIXEL_TRANSLATOR_FROMARGB_H
