//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_aligned.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_frame_aligned.h"

namespace aspia {

DesktopFrameAligned::DesktopFrameAligned(const QSize& size,
                                         const PixelFormat& format,
                                         int stride,
                                         quint8* data)
    : DesktopFrame(size, format, stride, data)
{
    // Nothing
}

DesktopFrameAligned::~DesktopFrameAligned()
{
    _aligned_free(data_);
}

// static
std::unique_ptr<DesktopFrameAligned> DesktopFrameAligned::Create(
    const QSize& size, const PixelFormat& format)
{
    int bytes_per_row = size.width() * format.bytesPerPixel();

    quint8* data = reinterpret_cast<quint8*>(_aligned_malloc(bytes_per_row * size.height(), 16));
    if (!data)
        return nullptr;

    return std::unique_ptr<DesktopFrameAligned>(
        new DesktopFrameAligned(size, format, bytes_per_row, data));
}

} // namespace aspia
