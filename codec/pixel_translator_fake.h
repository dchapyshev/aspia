/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator_fake.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_PIXEL_TRANSLATOR_FAKE_H
#define _ASPIA_PIXEL_TRANSLATOR_FAKE_H

#include "base/macros.h"
#include "codec/pixel_translator.h"

class PixelTranslatorFake : public PixelTranslator
{
public:
    PixelTranslatorFake() {}
    ~PixelTranslatorFake() {}

    virtual void Translate(const uint8_t *src,
                           int src_stride,
                           uint8_t *dst,
                           int dst_stride,
                           int width,
                           int height) override
    {
        for (int y = 0; y < height; ++y)
        {
            memcpy(dst, src, dst_stride);

            src += src_stride;
            dst += dst_stride;
        }
    }

private:
    DISALLOW_COPY_AND_ASSIGN(PixelTranslatorFake);
};

#endif // _ASPIA_PIXEL_TRANSLATOR_FAKE_H
