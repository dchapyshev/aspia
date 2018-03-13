//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_qimage.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_QIMAGE_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_QIMAGE_H

#include <QImage>
#include <memory>

#include "desktop_capture/desktop_frame.h"

namespace aspia {

class DesktopFrameQImage : public DesktopFrame
{
public:
    ~DesktopFrameQImage() = default;

    static std::unique_ptr<DesktopFrameQImage> Create(const QSize& size);

    const QImage& image() const;

private:
    DesktopFrameQImage(QImage&& img);

    QImage image_;

    DISALLOW_COPY_AND_ASSIGN(DesktopFrameQImage);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_QIMAGE_H
