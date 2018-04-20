//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_aligned.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_ALIGNED_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_ALIGNED_H

#include <memory>

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class DesktopFrameAligned : public DesktopFrame
{
public:
    ~DesktopFrameAligned();

    static std::unique_ptr<DesktopFrameAligned> Create(const QSize& size,
                                                       const PixelFormat& format);

private:
    DesktopFrameAligned(const QSize& size,
                      const PixelFormat& format,
                      int stride,
                      quint8* data);

    Q_DISABLE_COPY(DesktopFrameAligned)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_ALIGNED_H
