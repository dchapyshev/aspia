//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame_basic.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame_basic.h"
#include <malloc.h>

namespace aspia {

DesktopFrameBasic::DesktopFrameBasic(const DesktopSize& size,
                                     const PixelFormat& format,
                                     int stride,
                                     uint8_t* data) :
    DesktopFrame(size, format, stride, data)
{
    // Nothing
}

DesktopFrameBasic::~DesktopFrameBasic()
{
    free(data_);
}

// static
std::unique_ptr<DesktopFrameBasic> DesktopFrameBasic::Create(const DesktopSize& size,
                                                             const PixelFormat& format)
{
    int bytes_per_row = size.Width() * format.BytesPerPixel();

    uint8_t* data = reinterpret_cast<uint8_t*>(malloc(bytes_per_row * size.Height()));
    if (!data)
        return nullptr;

    return std::unique_ptr<DesktopFrameBasic>(new DesktopFrameBasic(size,
                                                                    format,
                                                                    bytes_per_row,
                                                                    data));
}

} // namespace aspia
