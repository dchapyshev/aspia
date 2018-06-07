//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_aligned.cc
// LICENSE:         GNU General Public License 3
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
    qFreeAligned(data_);
}

// static
std::unique_ptr<DesktopFrameAligned> DesktopFrameAligned::create(
    const QSize& size, const PixelFormat& format)
{
    int bytes_per_row = size.width() * format.bytesPerPixel();

    quint8* data = reinterpret_cast<quint8*>(qMallocAligned(bytes_per_row * size.height(), 16));
    if (!data)
        return nullptr;

    return std::unique_ptr<DesktopFrameAligned>(
        new DesktopFrameAligned(size, format, bytes_per_row, data));
}

} // namespace aspia
