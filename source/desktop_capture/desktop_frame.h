//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H

#include <QRegion>
#include <QPoint>
#include <QSize>

#include "desktop_capture/pixel_format.h"
#include "base/macros.h"

namespace aspia {

class DesktopFrame
{
public:
    virtual ~DesktopFrame() = default;

    uint8_t* GetFrameDataAtPos(const QPoint& pos) const;
    uint8_t* GetFrameDataAtPos(int32_t x, int32_t y) const;
    uint8_t* GetFrameData() const;
    const QSize& size() const;
    const PixelFormat& format() const;
    int stride() const;
    bool contains(int32_t x, int32_t y) const;

    const QRegion& UpdatedRegion() const;
    QRegion* MutableUpdatedRegion();

protected:
    DesktopFrame(const QSize& size, const PixelFormat& format,
                 int stride, uint8_t* data);

    // Ownership of the buffers is defined by the classes that inherit from
    // this class. They must guarantee that the buffer is not deleted before
    // the frame is deleted.
    uint8_t* const data_;

private:
    const QSize size_;
    const PixelFormat format_;
    const int stride_;

    QRegion updated_region_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrame);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
