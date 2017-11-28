//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_frame.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H

#include "desktop_capture/desktop_region.h"
#include "desktop_capture/pixel_format.h"
#include "base/macros.h"

namespace aspia {

class DesktopFrame
{
public:
    virtual ~DesktopFrame() = default;

    uint8_t* GetFrameDataAtPos(const DesktopPoint& pos) const;
    uint8_t* GetFrameDataAtPos(int32_t x, int32_t y) const;
    uint8_t* GetFrameData() const;
    const DesktopSize& Size() const;
    const PixelFormat& Format() const;
    int Stride() const;
    bool Contains(int32_t x, int32_t y) const;

    const DesktopRegion& UpdatedRegion() const;
    DesktopRegion* MutableUpdatedRegion();

protected:
    DesktopFrame(const DesktopSize& size, const PixelFormat& format,
                 int stride, uint8_t* data);

    // Ownership of the buffers is defined by the classes that inherit from
    // this class. They must guarantee that the buffer is not deleted before
    // the frame is deleted.
    uint8_t* const data_;

private:
    const DesktopSize size_;
    const PixelFormat format_;
    const int stride_;

    DesktopRegion updated_region_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrame);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
