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

namespace aspia {

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
                uint32_t red   = red_table_[*(uint32_t*)src >> src_format_.RedShift() & src_format_.RedMax()];
                uint32_t green = green_table_[*(uint32_t*)src >> src_format_.GreenShift() & src_format_.GreenMax()];
                uint32_t blue  = blue_table_[*(uint32_t*)src >> src_format_.BlueShift() & src_format_.BlueMax()];

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
        red_table_.resize(src_format_.RedMax() + 1);

        for (uint32_t i = 0; i <= src_format_.RedMax(); ++i)
        {
            red_table_[i] = ((i * dst_format.RedMax() + src_format_.RedMax() / 2) / src_format_.RedMax()) << dst_format.RedShift();
        }
    }

    void InitGreenTable(const PixelFormat &dst_format)
    {
        green_table_.resize(src_format_.GreenMax() + 1);

        for (uint32_t i = 0; i <= src_format_.GreenMax(); ++i)
        {
            green_table_[i] = ((i * dst_format.GreenMax() + src_format_.GreenMax() / 2) / src_format_.GreenMax()) << dst_format.GreenShift();
        }
    }

    void InitBlueTable(const PixelFormat &dst_format)
    {
        blue_table_.resize(src_format_.BlueMax() + 1);

        for (uint32_t i = 0; i <= src_format_.BlueMax(); ++i)
        {
            blue_table_[i] = ((i * dst_format.BlueMax() + src_format_.BlueMax() / 2) / src_format_.BlueMax()) << dst_format.BlueShift();
        }
    }

private:
    PixelFormat src_format_;

    std::vector<uint32_t> red_table_;
    std::vector<uint32_t> green_table_;
    std::vector<uint32_t> blue_table_;

    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFromARGB);
};

} // namespace aspia

#endif // _ASPIA_PIXEL_TRANSLATOR_FROMARGB_H
