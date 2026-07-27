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

#include "common/android/animation.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimerEvent>
#include <QWidget>

#include "common/android/controls.h"

namespace {

// Sane bounds for the tick interval against bogus refresh rates reported by the platform:
// 5 ms covers displays up to 200 Hz, 20 ms keeps at least 50 ticks per second.
constexpr int kMinTickInterval = 5;
constexpr int kMaxTickInterval = 20;

} // namespace

//--------------------------------------------------------------------------------------------------
Animation::Animation(QObject* parent)
    : QObject(parent)
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
Animation::~Animation() = default;

//--------------------------------------------------------------------------------------------------
void Animation::setDuration(MilliSeconds duration)
{
    duration_ = duration;
}

//--------------------------------------------------------------------------------------------------
void Animation::setEasingCurve(const QEasingCurve& easing)
{
    easing_ = easing;
}

//--------------------------------------------------------------------------------------------------
void Animation::setStartValue(double value)
{
    start_value_ = value;
}

//--------------------------------------------------------------------------------------------------
void Animation::setEndValue(double value)
{
    end_value_ = value;
}

//--------------------------------------------------------------------------------------------------
bool Animation::isRunning() const
{
    return timer_.isActive();
}

//--------------------------------------------------------------------------------------------------
void Animation::start()
{
    clock_.start();
    timer_.start(tickInterval(), Qt::PreciseTimer, this);
    emit sig_valueChanged(start_value_);
}

//--------------------------------------------------------------------------------------------------
void Animation::stop()
{
    timer_.stop();
}

//--------------------------------------------------------------------------------------------------
void Animation::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != timer_.timerId())
    {
        QObject::timerEvent(event);
        return;
    }

    double progress = 1.0;
    if (duration_.count() > 0)
        progress = qMin(1.0, static_cast<double>(clock_.elapsed()) / duration_.count());

    emit sig_valueChanged(
        Controls::lerp(start_value_, end_value_, easing_.valueForProgress(progress)));

    if (progress >= 1.0)
    {
        timer_.stop();
        emit sig_finished();
    }
}

//--------------------------------------------------------------------------------------------------
int Animation::tickInterval() const
{
    // The rate is sampled at every start: adaptive panels and battery saving change it at runtime.
    const QWidget* widget = qobject_cast<const QWidget*>(parent());
    const QScreen* screen = widget ? widget->screen() : QGuiApplication::primaryScreen();

    double rate = screen ? screen->refreshRate() : 0.0;
    if (rate <= 0.0)
        rate = 60.0;

    return qBound(kMinTickInterval, static_cast<int>(1000.0 / rate), kMaxTickInterval);
}
