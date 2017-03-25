//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame_basic.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame_basic.h"

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
    _aligned_free(data_);
}

// static
DesktopFrameBasic* DesktopFrameBasic::Create(const DesktopSize& size, const PixelFormat& format)
{
    int bytes_per_row = size.Width() * format.BytesPerPixel();

    uint8_t* data = reinterpret_cast<uint8_t*>(_aligned_malloc(bytes_per_row * size.Height(), 32));
    if (!data)
        return nullptr;

    return new DesktopFrameBasic(size, format, bytes_per_row, data);
}

} // namespace aspia
