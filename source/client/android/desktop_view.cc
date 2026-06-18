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

#include <QKeyEvent>
#include <QLineF>
#include <QPainter>
#include <QTimer>
#include <QTouchEvent>

#include "base/desktop/frame.h"
#include "base/desktop/mouse_cursor.h"
#include "common/keycode_converter.h"
#include "proto/desktop_input.h"

namespace {

const QColor kBackgroundColor(25, 25, 25);

constexpr qreal kMaxZoom = 5.0;

// Movement in widget pixels above which a touch is treated as a drag instead of a tap.
constexpr qreal kMoveThreshold = 8.0;

// A second finger-down within this interval after a tap starts a drag with the left button held.
constexpr qint64 kDoubleTapIntervalMs = 500;

// Holding a single finger still for this long also starts a left-button drag.
constexpr int kLongPressMs = 400;

// Edge band (widget pixels) where a held drag keeps auto-scrolling, the tick rate, and the maximum
// cursor advance per tick.
constexpr qreal kEdgeMargin = 80.0;
constexpr int kEdgeScrollIntervalMs = 16;
constexpr qreal kEdgeMaxSpeed = 18.0;

// Two-finger movement (widget pixels) needed to lock into zoom or scroll, and the finger travel per
// wheel step.
constexpr qreal kTwoFingerDecide = 16.0;
constexpr qreal kScrollStep = 40.0;

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopView::DesktopView(QWidget* parent)
    : QWidget(parent)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kBackgroundColor);
    setPalette(pal);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setFocusPolicy(Qt::StrongFocus);

    gesture_timer_.start();

    long_press_timer_ = new QTimer(this);
    long_press_timer_->setSingleShot(true);
    long_press_timer_->setInterval(kLongPressMs);
    connect(long_press_timer_, &QTimer::timeout, this, &DesktopView::onLongPress);

    edge_scroll_timer_ = new QTimer(this);
    edge_scroll_timer_->setInterval(kEdgeScrollIntervalMs);
    connect(edge_scroll_timer_, &QTimer::timeout, this, &DesktopView::onEdgeScroll);
}

//--------------------------------------------------------------------------------------------------
DesktopView::~DesktopView() = default;

//--------------------------------------------------------------------------------------------------
void DesktopView::setFrame(SharedFrame frame)
{
    const QSize previous_size = image_.size();

    frame_ = std::move(frame);

    if (frame_.isValid())
    {
        const SharedFrame::ReadAccess access = frame_.read();
        const Frame& source = access.get();
        const QSize& size = source.size();
        image_ = QImage(source.frameData(), size.width(), size.height(), source.stride(),
                        QImage::Format_RGB32);

        if (image_.size() != previous_size)
        {
            zoom_ = 1.0;
            cursor_pos_ = QPointF(image_.width() / 2.0, image_.height() / 2.0);
            clampContentPos();
            update();
        }
    }
    else
    {
        // The frame was cleared (on disconnect) and no draw frame follows, so repaint now.
        image_ = QImage();
        update();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopView::setCursorShape(std::shared_ptr<MouseCursor> cursor)
{
    if (cursor && cursor->isValid())
    {
        cursor_image_ = QImage(reinterpret_cast<const uchar*>(cursor->constImage().constData()),
                               cursor->width(), cursor->height(), cursor->stride(),
                               QImage::Format_ARGB32).copy();
        cursor_hotspot_ = cursor->hotSpot();
    }
    else
    {
        cursor_image_ = QImage();
        cursor_hotspot_ = QPoint();
    }

    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::refresh(const QList<QRect>& dirty_rects)
{
    if (image_.isNull() || dirty_rects.isEmpty())
    {
        update();
        return;
    }

    const qreal scale = contentScale();

    for (const QRect& frame_rect : dirty_rects)
    {
        // Map the host-frame rectangle to on-screen coordinates (accounting for zoom and pan),
        // expand by a pixel to cover smooth-scaling bleed, and clamp to the widget.
        const QRectF widget_rect(frameToWidget(frame_rect.topLeft()),
                                 QSizeF(frame_rect.width() * scale, frame_rect.height() * scale));
        update(widget_rect.toAlignedRect().adjusted(-1, -1, 1, 1) & rect());
    }
}

//--------------------------------------------------------------------------------------------------
bool DesktopView::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
            handleTouch(static_cast<QTouchEvent*>(event));
            return true;

        default:
            return QWidget::event(event);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopView::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.fillRect(rect(), kBackgroundColor);

    if (image_.isNull())
        return;

    clampContentPos();

    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    {
        // The IO thread writes this buffer in place; the read access holds the lock so we never
        // paint a half-written frame.
        const SharedFrame::ReadAccess lock = frame_.read();
        painter.drawImage(QRectF(content_pos_, fittedSize() * zoom_), image_);
    }

    const QPointF cursor_center = frameToWidget(cursor_pos_);
    if (!cursor_image_.isNull())
    {
        const qreal scale = contentScale();
        const QPointF hotspot(cursor_hotspot_.x() * scale, cursor_hotspot_.y() * scale);
        painter.drawImage(QRectF(cursor_center - hotspot, QSizeF(cursor_image_.size()) * scale),
                          cursor_image_);
    }
    else
    {
        // The host has not sent a usable cursor shape: draw a marker so the cursor stays visible.
        painter.setPen(QPen(Qt::black, 1));
        painter.setBrush(Qt::white);
        painter.drawEllipse(cursor_center, 6.0, 6.0);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopView::keyPressEvent(QKeyEvent* event)
{
    sendKey(event, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::keyReleaseEvent(QKeyEvent* event)
{
    sendKey(event, false);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setFocus();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::handleTouch(QTouchEvent* event)
{
    if (image_.isNull())
        return;

    QList<QPointF> down;
    QPointF sum;
    for (const QEventPoint& point : event->points())
    {
        if (point.state() == QEventPoint::Released)
            continue;
        down.append(point.position());
        sum += point.position();
    }

    const int count = down.size();

    // Gesture start and one-to-two finger transition.
    if (count >= 2 && (gesture_ == Gesture::NONE || gesture_ == Gesture::ONE_FINGER))
    {
        long_press_timer_->stop();
        edge_scroll_timer_->stop();

        // Release a held left button before a two-finger gesture so it never sticks down.
        if (dragging_)
        {
            button_mask_ = 0;
            dragging_ = false;
            sendMouse(0);
        }

        gesture_ = Gesture::TWO_FINGER;
        pinch_moved_ = false;
        pinch_centroid_ = sum / count;
        pinch_distance_ = QLineF(down[0], down[1]).length();
        two_finger_mode_ = TwoFingerMode::UNDECIDED;
        two_finger_start_centroid_ = pinch_centroid_;
        two_finger_start_distance_ = pinch_distance_;
        scroll_accumulator_ = 0.0;
    }
    else if (count == 1 && gesture_ == Gesture::NONE)
    {
        gesture_ = Gesture::ONE_FINGER;
        moved_ = false;
        last_point_ = down[0];

        // A finger-down soon after a tap arms a drag: if it moves, the left button is held.
        armed_drag_ = (gesture_timer_.elapsed() - last_tap_time_) <= kDoubleTapIntervalMs;
        dragging_ = false;
        button_mask_ = 0;

        // Holding the finger still also starts a drag.
        long_press_timer_->start();
    }

    // Gesture update.
    if (gesture_ == Gesture::ONE_FINGER && count == 1)
    {
        const QPointF delta = down[0] - last_point_;
        if (!moved_ && delta.manhattanLength() > kMoveThreshold)
        {
            moved_ = true;

            // Movement starts the armed drag, or ends the long-press wait for a plain cursor move.
            if (armed_drag_ && !dragging_)
                startDrag();
            else
                long_press_timer_->stop();
        }

        if (moved_)
        {
            moveCursorBy(delta);
            last_point_ = down[0];

            if (dragging_)
                updateEdgeScroll(down[0]);
        }
    }
    else if (gesture_ == Gesture::TWO_FINGER && count >= 2)
    {
        const QPointF centroid = sum / count;
        const qreal distance = QLineF(down[0], down[1]).length();

        const qreal distance_change = qAbs(distance - two_finger_start_distance_);
        const qreal centroid_move = (centroid - two_finger_start_centroid_).manhattanLength();

        if (distance_change > kMoveThreshold || centroid_move > kMoveThreshold)
            pinch_moved_ = true;

        // Lock the gesture to zoom or scroll once it has moved enough to tell them apart.
        if (two_finger_mode_ == TwoFingerMode::UNDECIDED &&
            (distance_change > kTwoFingerDecide || centroid_move > kTwoFingerDecide))
        {
            two_finger_mode_ = (distance_change > centroid_move) ?
                TwoFingerMode::ZOOM : TwoFingerMode::SCROLL;
        }

        if (two_finger_mode_ == TwoFingerMode::ZOOM)
        {
            if (pinch_distance_ > 0.0)
                applyZoom(distance / pinch_distance_, centroid);

            content_pos_ += centroid - pinch_centroid_;
            clampContentPos();

            // Keep the cursor on screen: zoom/pan must not leave it outside the visible area.
            ensureCursorVisible();
            update();
        }
        else if (two_finger_mode_ == TwoFingerMode::SCROLL)
        {
            // Fingers moving down scroll the content up, so it follows the fingers.
            scroll_accumulator_ += pinch_centroid_.y() - centroid.y();
            const int steps = static_cast<int>(scroll_accumulator_ / kScrollStep);
            if (steps != 0)
            {
                sendWheel(steps);
                scroll_accumulator_ -= steps * kScrollStep;
            }
        }

        pinch_centroid_ = centroid;
        pinch_distance_ = distance;
    }

    // Finger lifts: a tap that never turned into a drag is a click.
    if (gesture_ == Gesture::TWO_FINGER && count < 2)
    {
        if (!pinch_moved_)
            sendClick(proto::input::MouseEvent::RIGHT_BUTTON);
        gesture_ = (count == 0) ? Gesture::NONE : Gesture::IGNORED;
    }
    else if (gesture_ == Gesture::ONE_FINGER && count == 0)
    {
        if (!dragging_ && !moved_)
        {
            sendClick(proto::input::MouseEvent::LEFT_BUTTON);
            last_tap_time_ = gesture_timer_.elapsed();
        }
        gesture_ = Gesture::NONE;
    }
    else if (count == 0)
    {
        gesture_ = Gesture::NONE;
    }

    if (count == 0)
    {
        long_press_timer_->stop();
        edge_scroll_timer_->stop();
    }

    // Once every finger is up, never leave a button held: release the drag button so a missed
    // release can not jam the host with the left button stuck down.
    if (count == 0 && button_mask_ != 0)
    {
        button_mask_ = 0;
        dragging_ = false;
        sendMouse(0);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopView::startDrag()
{
    long_press_timer_->stop();
    dragging_ = true;
    button_mask_ = proto::input::MouseEvent::LEFT_BUTTON;

    // Press the left button at the current cursor position before the drag moves.
    sendMouse(button_mask_);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::onLongPress()
{
    // The finger has been held still long enough: grab the left button so the next moves drag.
    if (gesture_ == Gesture::ONE_FINGER && !moved_ && !dragging_)
        startDrag();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::updateEdgeScroll(const QPointF& finger_pos)
{
    auto axis_speed = [](qreal pos, qreal size) -> qreal
    {
        if (pos < kEdgeMargin)
            return -((kEdgeMargin - pos) / kEdgeMargin) * kEdgeMaxSpeed;
        if (pos > size - kEdgeMargin)
            return ((pos - (size - kEdgeMargin)) / kEdgeMargin) * kEdgeMaxSpeed;
        return 0.0;
    };

    edge_velocity_ = QPointF(qBound(-kEdgeMaxSpeed, axis_speed(finger_pos.x(), width()), kEdgeMaxSpeed),
                             qBound(-kEdgeMaxSpeed, axis_speed(finger_pos.y(), height()), kEdgeMaxSpeed));

    if (edge_velocity_.isNull())
        edge_scroll_timer_->stop();
    else if (!edge_scroll_timer_->isActive())
        edge_scroll_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::onEdgeScroll()
{
    if (!dragging_)
    {
        edge_scroll_timer_->stop();
        return;
    }

    moveCursorBy(edge_velocity_);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::moveCursorBy(const QPointF& widget_delta)
{
    const qreal scale = contentScale();
    if (scale <= 0.0)
        return;

    cursor_pos_ += widget_delta / scale;
    clampCursor();
    ensureCursorVisible();

    sendMouse(button_mask_);
    update();
}

//--------------------------------------------------------------------------------------------------
void DesktopView::applyZoom(qreal factor, const QPointF& anchor)
{
    const qreal previous_zoom = zoom_;
    zoom_ = qBound(1.0, zoom_ * factor, kMaxZoom);

    // Keep the point between the fingers anchored while scaling.
    const qreal scale = zoom_ / previous_zoom;
    content_pos_ = anchor - scale * (anchor - content_pos_);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::sendKey(QKeyEvent* event, bool pressed)
{
    quint32 usb_keycode = KeycodeConverter::nativeKeycodeToUsbKeycode(
        static_cast<int>(event->nativeScanCode()));

    if (usb_keycode == KeycodeConverter::invalidUsbKeycode())
        usb_keycode = KeycodeConverter::qtKeycodeToUsbKeycode(event->key());

    if (usb_keycode == KeycodeConverter::invalidUsbKeycode())
    {
        event->ignore();
        return;
    }

    proto::input::KeyEvent key_event;
    key_event.set_usb_keycode(usb_keycode);
    key_event.set_flags(pressed ? proto::input::KeyEvent::PRESSED : 0);

    emit sig_keyEvent(key_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::sendMouse(quint32 mask)
{
    if (image_.isNull())
        return;

    proto::input::MouseEvent event;
    event.set_x(qRound(cursor_pos_.x()));
    event.set_y(qRound(cursor_pos_.y()));
    event.set_mask(mask);

    emit sig_mouseEvent(event);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::sendClick(quint32 mask)
{
    sendMouse(mask);
    sendMouse(0);
}

//--------------------------------------------------------------------------------------------------
void DesktopView::sendWheel(int steps)
{
    if (image_.isNull() || steps == 0)
        return;

    // Positive steps scroll down, negative scroll up.
    const quint32 wheel = (steps > 0) ?
        proto::input::MouseEvent::WHEEL_DOWN : proto::input::MouseEvent::WHEEL_UP;

    proto::input::MouseEvent event;
    event.set_x(qRound(cursor_pos_.x()));
    event.set_y(qRound(cursor_pos_.y()));
    event.set_mask(wheel);

    const int count = qAbs(steps);
    for (int i = 0; i < count; ++i)
        emit sig_mouseEvent(event);
}

//--------------------------------------------------------------------------------------------------
QSizeF DesktopView::fittedSize() const
{
    return QSizeF(image_.size()).scaled(QSizeF(size()), Qt::KeepAspectRatio);
}

//--------------------------------------------------------------------------------------------------
qreal DesktopView::contentScale() const
{
    if (image_.isNull() || image_.width() == 0)
        return 1.0;
    return fittedSize().width() * zoom_ / image_.width();
}

//--------------------------------------------------------------------------------------------------
QPointF DesktopView::frameToWidget(const QPointF& frame_pos) const
{
    return content_pos_ + frame_pos * contentScale();
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

//--------------------------------------------------------------------------------------------------
void DesktopView::clampCursor()
{
    if (image_.isNull())
        return;

    cursor_pos_.setX(qBound(qreal(0), cursor_pos_.x(), qreal(image_.width() - 1)));
    cursor_pos_.setY(qBound(qreal(0), cursor_pos_.y(), qreal(image_.height() - 1)));
}

//--------------------------------------------------------------------------------------------------
void DesktopView::ensureCursorVisible()
{
    const QPointF pos = frameToWidget(cursor_pos_);

    qreal dx = 0.0;
    qreal dy = 0.0;
    if (pos.x() < 0.0)
        dx = -pos.x();
    else if (pos.x() > width())
        dx = width() - pos.x();
    if (pos.y() < 0.0)
        dy = -pos.y();
    else if (pos.y() > height())
        dy = height() - pos.y();

    content_pos_ += QPointF(dx, dy);
    clampContentPos();
}
