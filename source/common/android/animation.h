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

#ifndef COMMON_ANDROID_ANIMATION_H
#define COMMON_ANDROID_ANIMATION_H

#include <QBasicTimer>
#include <QEasingCurve>
#include <QElapsedTimer>
#include <QObject>

#include "base/time_types.h"

// Value animation for the touch controls. Unlike QVariantAnimation, which advances on the global
// animation timer fixed at ~60 ticks per second, this one ticks at the refresh rate of the screen
// its control is on, so a high refresh rate display gets a value for every frame. The value is
// sampled from the elapsed time, so timer jitter does not distort the motion.
class Animation final : public QObject
{
    Q_OBJECT

public:
    explicit Animation(QObject* parent = nullptr);
    ~Animation() final;

    void setDuration(MilliSeconds duration);
    void setEasingCurve(const QEasingCurve& easing);

    void setStartValue(double value);
    void setEndValue(double value);

    bool isRunning() const;

    // Starts from the beginning, publishing the start value right away. An already running
    // animation restarts.
    void start();

    // Stops without publishing sig_finished(); the value stays where the last tick left it.
    void stop();

signals:
    void sig_valueChanged(double value);
    void sig_finished();

protected:
    // QObject implementation.
    void timerEvent(QTimerEvent* event) final;

private:
    int tickInterval() const;

    QBasicTimer timer_;
    QElapsedTimer clock_;
    QEasingCurve easing_;
    MilliSeconds duration_{ 150 };
    double start_value_ = 0.0;
    double end_value_ = 1.0;

    Q_DISABLE_COPY_MOVE(Animation)
};

#endif // COMMON_ANDROID_ANIMATION_H
