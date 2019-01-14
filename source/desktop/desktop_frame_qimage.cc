//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/desktop_frame_qimage.h"

#include <QPixmap>

namespace desktop {

namespace {

constexpr int kBytesPerPixel = 4;

} // namespace

FrameQImage::FrameQImage(QImage&& img)
    : Frame(img.size(),
            PixelFormat::ARGB(),
            img.width() * kBytesPerPixel,
            img.bits()),
      image_(std::move(img))
{
    // Nothing
}

// static
std::unique_ptr<FrameQImage> FrameQImage::create(const QSize& size)
{
    return std::unique_ptr<FrameQImage>(
        new FrameQImage(QImage(size.width(), size.height(), QImage::Format_RGB32)));
}

// static
std::unique_ptr<FrameQImage> FrameQImage::create(const QPixmap& pixmap)
{
    return std::unique_ptr<FrameQImage>(new FrameQImage(pixmap.toImage()));
}

// static
std::unique_ptr<FrameQImage> FrameQImage::create(QImage&& image)
{
    return std::unique_ptr<FrameQImage>(new FrameQImage(std::move(image)));
}

} // namespace desktop
