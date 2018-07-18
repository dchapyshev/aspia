//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H

#include <QRegion>
#include <QPoint>
#include <QSize>

#include "desktop_capture/pixel_format.h"

namespace aspia {

class DesktopFrame
{
public:
    virtual ~DesktopFrame() = default;

    quint8* frameDataAtPos(const QPoint& pos) const;
    quint8* frameDataAtPos(int x, int y) const;
    quint8* frameData() const { return data_; }
    const QSize& size() const { return size_; }
    const PixelFormat& format() const { return format_; }
    int stride() const { return stride_; }
    bool contains(int x, int y) const;

    const QRegion& constUpdatedRegion() const { return updated_region_; }
    QRegion* updatedRegion() { return &updated_region_; }

    const QPoint& topLeft() const { return top_left_; }
    void setTopLeft(const QPoint& top_left) { top_left_ = top_left; }

protected:
    DesktopFrame(const QSize& size, const PixelFormat& format, int stride, quint8* data);

    // Ownership of the buffers is defined by the classes that inherit from
    // this class. They must guarantee that the buffer is not deleted before
    // the frame is deleted.
    quint8* const data_;

private:
    const QSize size_;
    const PixelFormat format_;
    const int stride_;

    QRegion updated_region_;
    QPoint top_left_;

    Q_DISABLE_COPY(DesktopFrame)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_H
