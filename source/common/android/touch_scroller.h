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

#ifndef COMMON_ANDROID_TOUCH_SCROLLER_H
#define COMMON_ANDROID_TOUCH_SCROLLER_H

#include <QEvent>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QPointF>

class QAbstractScrollArea;
class QTimer;
class QWidget;
class Animation;

// Finger scrolling for a QAbstractScrollArea. The content follows the finger, a flick keeps it
// moving with constant deceleration, and a tap reaches the control under the finger as a mouse
// press and release. Vertical only. Attaches itself to the scroll area passed to the constructor.
class TouchScroller final : public QObject
{
    Q_OBJECT

public:
    explicit TouchScroller(QAbstractScrollArea* area);
    ~TouchScroller() final;

    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private slots:
    void onPressDelayTimeout();
    void onFlingValueChanged(double value);

private:
    enum class State
    {
        IDLE,
        PRESSED,
        DRAGGING
    };

    struct Sample
    {
        qint64 time;
        double y;
    };

    void handlePress(const QPointF& global_pos, qint64 time);
    void handleMove(const QPointF& global_pos, qint64 time);
    void handleRelease(const QPointF& global_pos, qint64 time);
    void handleCancel();
    void addSample(qint64 time, double y);
    void scrollBy(double delta);
    void startFling(double velocity);

    // Finger velocity in pixels per millisecond over the recent samples.
    double releaseVelocity() const;

    void deliverPress();
    void deliverRelease(const QPointF& global_pos);

    // Lets go of a pressed control without a click.
    void cancelPress();

    void sendMouseEvent(QEvent::Type type, const QPointF& global_pos);

    QAbstractScrollArea* area_;
    QTimer* press_timer_;
    Animation* fling_;

    State state_ = State::IDLE;
    int touch_id_ = -1;
    QPointF press_pos_;
    QPointF last_pos_;
    double pending_scroll_ = 0.0;
    QList<Sample> samples_;
    QPointer<QWidget> press_target_;
    bool press_delivered_ = false;
    bool sending_ = false;

    Q_DISABLE_COPY_MOVE(TouchScroller)
};

#endif // COMMON_ANDROID_TOUCH_SCROLLER_H
