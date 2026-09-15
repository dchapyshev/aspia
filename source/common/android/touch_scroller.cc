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

#include "common/android/touch_scroller.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QEasingCurve>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QTouchEvent>

#include "base/time_types.h"
#include "common/android/animation.h"

namespace {

// Distances are in logical pixels, times in milliseconds.
constexpr int kDragThreshold = 8;
constexpr MilliSeconds kPressDelay{ 100 };
constexpr qint64 kVelocityWindow = 100;
constexpr double kMinFlingVelocity = 0.05;
constexpr double kMaxFlingVelocity = 8.0;
constexpr double kFlingDeceleration = 0.004;

} // namespace

//--------------------------------------------------------------------------------------------------
TouchScroller::TouchScroller(QAbstractScrollArea* area)
    : QObject(area),
      area_(area),
      press_timer_(new QTimer(this)),
      fling_(new Animation(this))
{
    // Touches are taken on the viewport, so a finger landing on a child control still scrolls. The
    // control gets a tap as mouse events afterwards. A popup does not receive touches, only the mouse
    // events Qt synthesizes from them, so those are handled the same way.
    QWidget* viewport = area_->viewport();
    viewport->setAttribute(Qt::WA_AcceptTouchEvents);
    viewport->installEventFilter(this);

    press_timer_->setSingleShot(true);
    press_timer_->setInterval(kPressDelay);
    connect(press_timer_, &QTimer::timeout, this, &TouchScroller::onPressDelayTimeout);

    fling_->setEasingCurve(QEasingCurve::OutQuad);
    connect(fling_, &Animation::sig_valueChanged, this, &TouchScroller::onFlingValueChanged);
}

//--------------------------------------------------------------------------------------------------
TouchScroller::~TouchScroller() = default;

//--------------------------------------------------------------------------------------------------
bool TouchScroller::eventFilter(QObject* object, QEvent* event)
{
    if (object != area_->viewport() || sending_)
        return false;

    switch (event->type())
    {
        case QEvent::TouchBegin:
        {
            const QTouchEvent* touch = static_cast<const QTouchEvent*>(event);
            const QEventPoint& point = touch->points().first();
            touch_id_ = point.id();
            handlePress(point.globalPosition(), touch->timestamp());
            return true;
        }

        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        {
            const QTouchEvent* touch = static_cast<const QTouchEvent*>(event);
            const QList<QEventPoint>& points = touch->points();
            for (const QEventPoint& point : points)
            {
                if (point.id() != touch_id_)
                    continue;

                if (point.state() == QEventPoint::Released)
                    handleRelease(point.globalPosition(), touch->timestamp());
                else
                    handleMove(point.globalPosition(), touch->timestamp());
                break;
            }
            return true;
        }

        case QEvent::TouchCancel:
            handleCancel();
            return true;

        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        {
            const QMouseEvent* mouse = static_cast<const QMouseEvent*>(event);
            if (mouse->button() != Qt::LeftButton)
                return false;

            handlePress(mouse->globalPosition(), mouse->timestamp());
            return true;
        }

        case QEvent::MouseMove:
        {
            const QMouseEvent* mouse = static_cast<const QMouseEvent*>(event);
            if (state_ == State::IDLE || !(mouse->buttons() & Qt::LeftButton))
                return false;

            handleMove(mouse->globalPosition(), mouse->timestamp());
            return true;
        }

        case QEvent::MouseButtonRelease:
        {
            const QMouseEvent* mouse = static_cast<const QMouseEvent*>(event);
            if (state_ == State::IDLE || mouse->button() != Qt::LeftButton)
                return false;

            handleRelease(mouse->globalPosition(), mouse->timestamp());
            return true;
        }

        default:
            return false;
    }
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::onPressDelayTimeout()
{
    deliverPress();
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::onFlingValueChanged(double value)
{
    area_->verticalScrollBar()->setValue(qRound(value));
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::handlePress(const QPointF& global_pos, qint64 time)
{
    if (state_ != State::IDLE)
        handleCancel();

    const bool flinging = fling_->isRunning();
    fling_->stop();

    state_ = State::PRESSED;
    press_pos_ = global_pos;
    last_pos_ = global_pos;
    pending_scroll_ = 0.0;
    samples_.clear();
    addSample(time, global_pos.y());

    // A finger that stops a flick is not a tap on whatever came to rest under it.
    if (flinging)
        return;

    QWidget* viewport = area_->viewport();
    press_target_ = viewport->childAt(viewport->mapFromGlobal(global_pos).toPoint());
    if (!press_target_)
        press_target_ = viewport;
    press_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::handleMove(const QPointF& global_pos, qint64 time)
{
    if (state_ == State::IDLE)
        return;

    addSample(time, global_pos.y());

    if (state_ == State::PRESSED)
    {
        const QPointF delta = global_pos - press_pos_;
        if (qAbs(delta.y()) <= kDragThreshold || qAbs(delta.y()) < qAbs(delta.x()))
            return;

        state_ = State::DRAGGING;
        cancelPress();

        // The threshold distance is not scrolled, so the content does not jump when the drag starts.
        last_pos_ = press_pos_;
        last_pos_.ry() += (delta.y() > 0) ? kDragThreshold : -kDragThreshold;
    }

    scrollBy(global_pos.y() - last_pos_.y());
    last_pos_ = global_pos;
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::handleRelease(const QPointF& global_pos, qint64 time)
{
    if (state_ == State::IDLE)
        return;

    addSample(time, global_pos.y());

    if (state_ == State::PRESSED)
    {
        state_ = State::IDLE;
        press_timer_->stop();

        if (press_target_)
        {
            if (!press_delivered_)
                deliverPress();
            deliverRelease(global_pos);
        }
        return;
    }

    scrollBy(global_pos.y() - last_pos_.y());
    state_ = State::IDLE;

    // The content moves against the finger.
    const double velocity = -releaseVelocity();
    if (qAbs(velocity) >= kMinFlingVelocity)
        startFling(qBound(-kMaxFlingVelocity, velocity, kMaxFlingVelocity));
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::handleCancel()
{
    if (state_ == State::IDLE)
        return;

    state_ = State::IDLE;
    cancelPress();
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::addSample(qint64 time, double y)
{
    samples_.append({ time, y });

    while (time - samples_.first().time > kVelocityWindow)
        samples_.removeFirst();
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::scrollBy(double delta)
{
    pending_scroll_ -= delta;

    const int step = qRound(pending_scroll_);
    pending_scroll_ -= step;

    QScrollBar* bar = area_->verticalScrollBar();
    bar->setValue(bar->value() + step);
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::startFling(double velocity)
{
    const QScrollBar* bar = area_->verticalScrollBar();
    const double speed = qAbs(velocity);
    const double start = bar->value();

    // With constant deceleration the flick covers v^2 / 2a. Where the range ends first, the flick
    // ends there. Either way it starts at the release speed, which the duration 2d / v gives the
    // OutQuad curve.
    double end = start + velocity * speed / (2.0 * kFlingDeceleration);
    end = qBound<double>(bar->minimum(), end, bar->maximum());
    if (end == start)
        return;

    fling_->setDuration(MilliSeconds(qRound(2.0 * qAbs(end - start) / speed)));
    fling_->setStartValue(start);
    fling_->setEndValue(end);
    fling_->start();
}

//--------------------------------------------------------------------------------------------------
double TouchScroller::releaseVelocity() const
{
    // A least squares fit over the samples of the last kVelocityWindow milliseconds. The last
    // sample alone would decide the flick otherwise, and the finger rolling off the screen makes
    // that one point anywhere.
    const int count = samples_.size();
    if (count < 2)
        return 0.0;

    double mean_time = 0.0;
    double mean_y = 0.0;
    for (const Sample& sample : samples_)
    {
        mean_time += sample.time;
        mean_y += sample.y;
    }
    mean_time /= count;
    mean_y /= count;

    double variance = 0.0;
    double covariance = 0.0;
    for (const Sample& sample : samples_)
    {
        const double dt = sample.time - mean_time;
        variance += dt * dt;
        covariance += dt * (sample.y - mean_y);
    }

    if (variance <= 0.0)
        return 0.0;

    return covariance / variance;
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::deliverPress()
{
    if (!press_target_)
        return;

    press_delivered_ = true;

    // Focus goes where a real click would put it, to the nearest enclosing control that takes
    // focus on click.
    QPoint local = press_target_->mapFromGlobal(press_pos_).toPoint();
    for (QWidget* widget = press_target_; widget; widget = widget->parentWidget())
    {
        if (widget->isEnabled() && widget->rect().contains(local) &&
            (widget->focusPolicy() & Qt::ClickFocus))
        {
            if (!widget->hasFocus())
                widget->setFocus(Qt::MouseFocusReason);
            break;
        }

        if (widget->isWindow())
            break;

        local += widget->pos();
    }

    sendMouseEvent(QEvent::MouseButtonPress, press_pos_);
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::deliverRelease(const QPointF& global_pos)
{
    sendMouseEvent(QEvent::MouseButtonRelease, global_pos);
    press_delivered_ = false;
    press_target_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::cancelPress()
{
    press_timer_->stop();

    // A release far outside the control ends the press without a click.
    if (press_delivered_)
        deliverRelease(QPointF(-QWIDGETSIZE_MAX, -QWIDGETSIZE_MAX));

    press_target_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void TouchScroller::sendMouseEvent(QEvent::Type type, const QPointF& global_pos)
{
    QWidget* target = press_target_;
    if (!target)
        return;

    const Qt::MouseButtons buttons =
        (type == QEvent::MouseButtonPress) ? Qt::LeftButton : Qt::NoButton;
    QMouseEvent event(type, target->mapFromGlobal(global_pos),
                      target->window()->mapFromGlobal(global_pos), global_pos, Qt::LeftButton,
                      buttons, Qt::NoModifier, Qt::MouseEventSynthesizedByApplication);

    sending_ = true;
    QApplication::sendEvent(target, &event);
    sending_ = false;
}
