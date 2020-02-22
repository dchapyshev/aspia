//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT__UI__FRAME_QIMAGE_H
#define CLIENT__UI__FRAME_QIMAGE_H

#include "desktop/desktop_frame.h"

#include <QImage>

#include <memory>

namespace client {

class FrameQImage : public desktop::Frame
{
public:
    ~FrameQImage() = default;

    static std::unique_ptr<FrameQImage> create(const desktop::Size& size);
    static std::unique_ptr<FrameQImage> create(const QPixmap& pixmap);
    static std::unique_ptr<FrameQImage> create(QImage&& image);

    const QImage& constImage() const { return image_; }
    QImage* image() { return &image_; }

private:
    FrameQImage(QImage&& img);

    QImage image_;

    DISALLOW_COPY_AND_ASSIGN(FrameQImage);
};

} // namespace client

#endif // CLIENT__UI__FRAME_QIMAGE_H
