//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame_basic.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_BASIC_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_BASIC_H

#include "desktop_capture/desktop_frame.h"

#include <memory>

namespace aspia {

class DesktopFrameBasic : public DesktopFrame
{
public:
    ~DesktopFrameBasic();

    static std::unique_ptr<DesktopFrameBasic> Create(const DesktopSize& size,
                                                     const PixelFormat& format);

private:
    DesktopFrameBasic(const DesktopSize& size,
                      const PixelFormat& format,
                      int stride,
                      uint8_t* data);

    DISALLOW_COPY_AND_ASSIGN(DesktopFrameBasic);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_BASIC_H
