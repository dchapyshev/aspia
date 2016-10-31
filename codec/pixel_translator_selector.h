/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_selector.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_SELECTOR_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_SELECTOR_H

#include "codec/pixel_translator_fromARGB.h"
#include "codec/pixel_translator_fromRGB565.h"
#include "codec/pixel_translator_fake.h"
#include "codec/pixel_translator_libyuv.h"

static std::unique_ptr<PixelTranslator> SelectTranslator(const PixelFormat &src_format,
                                                         const PixelFormat &dst_format)
{
    std::unique_ptr<PixelTranslator> translator(nullptr);

    if (src_format.IsEqualTo(dst_format))
    {
        if (dst_format.IsEqualTo(PixelFormat::MakeARGB()))
        {
            translator.reset(new PixelTranslatorARGBtoARGB());
        }
        else
        {
            translator.reset(new PixelTranslatorFake());
        }
    }
    else if (src_format.bits_per_pixel() == 32)
    {
        if (dst_format.bits_per_pixel() == 8)
        {
            translator.reset(new PixelTranslatorFromARGB<uint8_t>(src_format,
                                                                  dst_format));
        }
        else if (dst_format.bits_per_pixel() == 16)
        {
            if (dst_format.IsEqualTo(PixelFormat::MakeRGB565()))
            {
                translator.reset(new PixelTranslatorARGBtoRGB565());
            }
            else
            {
                translator.reset(new PixelTranslatorFromARGB<uint16_t>(src_format,
                                                                       dst_format));
            }
        }
        else if (dst_format.bits_per_pixel() == 32)
        {
            translator.reset(new PixelTranslatorFromARGB<uint32_t>(src_format,
                                                                   dst_format));
        }
    }
    else if (src_format.bits_per_pixel() == 16)
    {
        if (dst_format.bits_per_pixel() == 8)
        {
            translator.reset(new PixelTranslatorFromRGB565<uint8_t>(src_format,
                                                                    dst_format));
        }
        else if (dst_format.bits_per_pixel() == 16)
        {
            translator.reset(new PixelTranslatorFromRGB565<uint16_t>(src_format,
                                                                     dst_format));
        }
        else if (dst_format.bits_per_pixel() == 32)
        {
            if (src_format.IsEqualTo(PixelFormat::MakeRGB565()) &&
                dst_format.IsEqualTo(PixelFormat::MakeARGB()))
            {
                translator.reset(new PixelTranslatorRGB565toARGB());
            }
            else
            {
                translator.reset(new PixelTranslatorFromRGB565<uint32_t>(src_format,
                                                                         dst_format));
            }
        }
    }

    return translator;
}

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_SELECTOR_H
