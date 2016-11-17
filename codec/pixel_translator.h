/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/pixel_translator.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_H

#include "aspia_config.h"

#include "desktop_capture/pixel_format.h"

class PixelTranslator
{
public:
    PixelTranslator() {}
    virtual ~PixelTranslator() {}

    virtual void Translate(const uint8_t *src,
                           int src_stride,
                           uint8_t *dst,
                           int dst_stride,
                           int width,
                           int height) = 0;
};

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_H
