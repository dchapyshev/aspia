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

#include <QGestureEvent>
#include <QMouseEvent>
#include <QPainter>

#include "base/desktop/frame.h"

namespace {

constexpr qreal kMaxZoom = 5.0;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopView::DesktopView(QWidget* parent)
    : QWidget(parent)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);

    grabGesture(Qt::PinchGesture);
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
bool DesktopView::event(QEvent* event)
{
    if (event->type() == QEvent::Gesture)
    {
        QGestureEvent* gesture_event = static_cast<QGestureEvent*>(event);
        if (QGesture* gesture = gesture_event->gesture(Qt::PinchGesture))
        {
            handlePinch(static_cast<QPinchGesture*>(gesture));
            return true;
        }
    }

    return QWidget::event(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (image_.isNull())
        return;

    clampContentPos();

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(content_pos_, fittedSize() * zoom_), image_);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::mousePressEvent(QMouseEvent* event)
{
    // A single-finger drag pans the frame while it is zoomed in.
    if (zoom_ > 1.0)
    {
        panning_ = true;
        pan_last_ = event->position().toPoint();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopView::mouseMoveEvent(QMouseEvent* event)
{
    if (!panning_)
        return;

    const QPoint pos = event->position().toPoint();
    content_pos_ += pos - pan_last_;
    pan_last_ = pos;

    clampContentPos();
    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::mouseReleaseEvent(QMouseEvent* /* event */)
{
    panning_ = false;
}

//--------------------------------------------------------------------------------------------------
void DesktopView::handlePinch(QPinchGesture* gesture)
{
    const QPinchGesture::ChangeFlags flags = gesture->changeFlags();

    if (flags & QPinchGesture::ScaleFactorChanged)
    {
        const qreal previous_zoom = zoom_;
        zoom_ = qBound(1.0, zoom_ * gesture->scaleFactor(), kMaxZoom);

        // Keep the point under the fingers anchored while scaling.
        const qreal scale = zoom_ / previous_zoom;
        const QPointF anchor = mapFromGlobal(gesture->centerPoint().toPoint());
        content_pos_ = anchor - scale * (anchor - content_pos_);
    }

    if (flags & QPinchGesture::CenterPointChanged)
        content_pos_ += gesture->centerPoint() - gesture->lastCenterPoint();

    clampContentPos();
    update();
}

//--------------------------------------------------------------------------------------------------
QSizeF DesktopView::fittedSize() const
{
    return QSizeF(image_.size()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::clampContentPos()
{
    const QSizeF content = fittedSize() * zoom_;

    if (content.width() <= width())
        content_pos_.setX((width() - content.width()) / 2.0);
    else
        content_pos_.setX(qBound(width() - content.width(), content_pos_.x(), qreal(0)));

    if (content.height() <= height())
        content_pos_.setY((height() - content.height()) / 2.0);
    else
        content_pos_.setY(qBound(height() - content.height(), content_pos_.y(), qreal(0)));
}
