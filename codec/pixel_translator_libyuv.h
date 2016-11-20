/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_libyuv.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_LIBYUV_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_LIBYUV_H

#include "pixel_translator.h"

#include "libyuv/convert_argb.h"
#include "libyuv/convert_from_argb.h"

class PixelTranslatorARGBtoARGB : public PixelTranslator
{
public:
    PixelTranslatorARGBtoARGB()
    {
        // Nothing
    }

    virtual ~PixelTranslatorARGBtoARGB()
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
        libyuv::ARGBToARGB(src, src_stride, dst, dst_stride, width, height);
    }
};

class PixelTranslatorARGBtoRGB565 : public PixelTranslator
{
public:
    PixelTranslatorARGBtoRGB565()
    {
        // Nothing
    }

    virtual ~PixelTranslatorARGBtoRGB565()
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
        libyuv::ARGBToRGB565(src, src_stride, dst, dst_stride, width, height);
    }
};

class PixelTranslatorRGB565toARGB : public PixelTranslator
{
public:
    PixelTranslatorRGB565toARGB()
    {
        // Nothing
    }

    virtual ~PixelTranslatorRGB565toARGB()
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
        libyuv::RGB565ToARGB(src, src_stride, dst, dst_stride, width, height);
    }
};

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_LIBYUV_H
