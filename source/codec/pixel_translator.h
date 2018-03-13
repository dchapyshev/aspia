//
// PROJECT:         Aspia
// FILE:            codec/pixel_translator.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__PIXEL_TRANSLATOR_H
#define _ASPIA_CODEC__PIXEL_TRANSLATOR_H

#include <memory>
#include <vector>

#include "desktop_capture/pixel_format.h"

namespace aspia {

class PixelTranslator
{
public:
    virtual ~PixelTranslator() = default;

    static std::unique_ptr<PixelTranslator> Create(const PixelFormat& source_format,
                                                   const PixelFormat& target_format);

    virtual void Translate(const uint8_t* src,
                           int src_stride,
                           uint8_t* dst,
                           int dst_stride,
                           int width,
                           int height) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__PIXEL_TRANSLATOR_H
