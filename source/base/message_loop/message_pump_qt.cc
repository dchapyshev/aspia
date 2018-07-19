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

#include "base/message_loop/message_pump_qt.h"

#include <QApplication>
#include <QEvent>

namespace aspia {

namespace {

const int kHaveWorkEvent = QEvent::User + 1;
const int kMinTimerInterval = 10; // ms

} // namespace

//================================================================================================
// MessagePumpForQt implementation.
//================================================================================================

MessagePumpForQt::MessagePumpForQt()
{
    // QApplication instance must exist.
    Q_ASSERT(QApplication::instance());
}

int MessagePumpForQt::run(Delegate* delegate)
{
    RunState state;
    state.delegate    = delegate;
    state.should_quit = false;

    state_ = &state;
    int exit_code = doRunLoop();
    state_ = nullptr;

    return exit_code;
}

void MessagePumpForQt::quit()
{
    Q_ASSERT(state_);
    state_->should_quit = true;
    event_loop_->quit();
}

void MessagePumpForQt::scheduleWork()
{
    QApplication::postEvent(event_loop_, new QEvent(QEvent::Type(kHaveWorkEvent)));
}

void MessagePumpForQt::scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time)
{
    delayed_work_time_ = delayed_work_time;

    int delay_msec = currentDelay();
    Q_ASSERT(delay_msec > 0);
    if (delay_msec < kMinTimerInterval)
        delay_msec = kMinTimerInterval;

    // Create a timer event that will wake us up to check for any pending timers (in case we are
    // running within a nested, external sub-pump).
    Q_ASSERT(!event_loop_timer_);
    event_loop_timer_ = event_loop_->startTimer(delay_msec);
}

int MessagePumpForQt::doRunLoop()
{
    EventLoop event_loop(this);
    event_loop_ = &event_loop;

    for (;;)
    {
        bool more_work_is_plausible = !event_loop_->processEvents(QEventLoop::AllEvents);
        if (state_->should_quit)
            break;

        more_work_is_plausible |= state_->delegate->doWork();
        if (state_->should_quit)
            break;

        more_work_is_plausible |= state_->delegate->doDelayedWork(delayed_work_time_);

        // If we did not process any delayed work, then we can assume that our existing timer if
        // any will fire when delayed work should run. We don't want to disturb that timer if it is
        // already in flight. However, if we did do all remaining delayed work, then lets kill the
        // timer.
        if (more_work_is_plausible && delayed_work_time_ == MessageLoopTimePoint())
        {
            event_loop_->killTimer(event_loop_timer_);
            event_loop_timer_ = 0;
        }

        if (state_->should_quit)
            break;

        if (more_work_is_plausible)
            continue;

        // Wait (sleep) until we have work to do again.
        event_loop_->exec(QEventLoop::WaitForMoreEvents);
    }

    event_loop_ = nullptr;
    return 0;
}

void MessagePumpForQt::handleWorkMessage()
{
    if (!state_)
        return;

    // Now give the delegate a chance to do some work. He'll let us know if he needs to do more work.
    if (state_->delegate->doWork())
        scheduleWork();
}

void MessagePumpForQt::handleTimerMessage()
{
    event_loop_->killTimer(event_loop_timer_);
    event_loop_timer_ = 0;

    if (!state_)
        return;

    state_->delegate->doDelayedWork(delayed_work_time_);
    if (delayed_work_time_ != MessageLoopTimePoint())
    {
        // A bit gratuitous to set delayed_work_time_ again, but oh well.
        scheduleDelayedWork(delayed_work_time_);
    }
}

int MessagePumpForQt::currentDelay() const
{
    if (delayed_work_time_ == MessageLoopTimePoint())
        return -1;

    // Be careful here. TimeDelta has a precision of microseconds, but we want a value in
    // milliseconds. If there are 5.5ms left, should the delay be 5 or 6?  It should be 6 to avoid
    // executing delayed work too early.
    MessageLoopTimePoint current_time = MessageLoopClock::now();

    int64_t timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        delayed_work_time_ - current_time).count();

    // If this value is negative, then we need to run delayed work soon.
    int delay = static_cast<int>(timeout);
    if (delay < 0)
        delay = 0;

    return delay;
}

//================================================================================================
// MessagePumpForQt::EventLoop implementation.
//================================================================================================

MessagePumpForQt::EventLoop::EventLoop(MessagePumpForQt* pump)
    : pump_(pump)
{
    // Nothing
}

void MessagePumpForQt::EventLoop::customEvent(QEvent* event)
{
    if (event->type() == kHaveWorkEvent)
        pump_->handleWorkMessage();
}

void MessagePumpForQt::EventLoop::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == pump_->event_loop_timer_)
        pump_->handleTimerMessage();
}

} // namespace aspia
