//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_frame_qimage.h
// LICENSE:         GNU General Public License 3
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

    static std::unique_ptr<DesktopFrameQImage> create(const QSize& size);
    static std::unique_ptr<DesktopFrameQImage> create(const QPixmap& pixmap);
    static std::unique_ptr<DesktopFrameQImage> create(QImage&& image);

    const QImage& constImage() const { return image_; }
    QImage* image() { return &image_; }

private:
    DesktopFrameQImage(QImage&& img);

    QImage image_;

    Q_DISABLE_COPY(DesktopFrameQImage)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_FRAME_QIMAGE_H
