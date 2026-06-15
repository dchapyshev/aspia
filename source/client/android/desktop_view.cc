//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/android/desktop_view.h"

#include <QPainter>

#include "base/desktop/frame.h"

//--------------------------------------------------------------------------------------------------
DesktopView::DesktopView(QWidget* parent)
    : QWidget(parent)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);
}

//--------------------------------------------------------------------------------------------------
DesktopView::~DesktopView() = default;

//--------------------------------------------------------------------------------------------------
void DesktopView::setFrame(std::shared_ptr<Frame> frame)
{
    frame_ = std::move(frame);

    if (frame_)
    {
        const QSize& size = frame_->size();
        image_ = QImage(frame_->frameData(), size.width(), size.height(), frame_->stride(),
                        QImage::Format_RGB32);
    }
    else
    {
        image_ = QImage();
    }

    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::refresh()
{
    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (image_.isNull())
        return;

    // Fit the frame into the widget preserving the aspect ratio, centered.
    const QSize scaled = image_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2),
                       scaled);

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, image_);
}
