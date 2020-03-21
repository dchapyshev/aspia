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

#include "base/message_loop/message_pump_win.h"

#include "base/logging.h"

namespace base {

// Message sent to get an additional time slice for pumping (processing) another task (a series of
// such messages creates a continuous task pump).
static const UINT kMsgHaveWork = WM_USER + 1;

MessagePumpForWin::MessagePumpForWin()
{
    bool succeeded = message_window_.create(
        std::bind(&MessagePumpForWin::onMessage,
                  this,
                  std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4));
    DCHECK(succeeded);
}

void MessagePumpForWin::run(Delegate* delegate)
{
    runWithDispatcher(delegate, nullptr);
}

void MessagePumpForWin::quit()
{
    DCHECK(state_);
    state_->should_quit = true;
}

void MessagePumpForWin::scheduleWork()
{
    if (InterlockedExchange(&work_state_, HAVE_WORK) != READY)
        return; // Someone else continued the pumping.

    // Make sure the MessagePump does some work for us.
    BOOL ret = PostMessageW(message_window_.hwnd(), kMsgHaveWork, 0, 0);
    if (ret)
        return; // There was room in the Window Message queue.

    // We have failed to insert a have-work message, so there is a chance that we will starve
    // tasks/timers while sitting in a nested message loop. Nested loops only look at Windows
    // Message queues, and don't look at *our* task queues, etc., so we might not get a time slice
    // in such. :-(
    // We could abort here, but the fear is that this failure mode is plausibly common (queue is
    // full, of about 2000 messages), so we'll do a near-graceful recovery.  Nested loops are
    // pretty transient (we think), so this will probably be recoverable.
    // Clarify that we didn't really insert.
    InterlockedExchange(&work_state_, READY);
}

void MessagePumpForWin::scheduleDelayedWork(TimePoint delayed_work_time)
{
    // We would *like* to provide high resolution timers. Windows timers using SetTimer() have a
    // 10ms granularity. We have to use WM_TIMER as a wakeup mechanism because the application can
    // enter modal windows loops where it is not running our MessageLoop; the only way to have our
    // timers fire in these cases is to post messages there.
    //
    // To provide sub-10ms timers, we process timers directly from our run loop. For the common
    // case, timers will be processed there as the run loop does its normal work.  However, we
    // *also* set the system timer so that WM_TIMER events fire.  This mops up the case of timers
    // not being able to work in modal message loops.  It is possible for the SetTimer to pop and
    // have no pending timers, because they could have already been processed by the run loop itself.
    //
    // We use a single SetTimer corresponding to the timer that will expire soonest. As new timers
    // are created and destroyed, we update SetTimer. Getting a spurrious SetTimer event firing is
    // benign, as we'll just be processing an empty timer queue.
    delayed_work_time_ = delayed_work_time;

    int delay_msec = currentDelay();
    DCHECK(delay_msec > 0);
    if (delay_msec < USER_TIMER_MINIMUM)
        delay_msec = USER_TIMER_MINIMUM;

    // Create a WM_TIMER event that will wake us up to check for any pending timers (in case we are
    // running within a nested, external sub-pump).
    SetTimer(message_window_.hwnd(), reinterpret_cast<UINT_PTR>(this), delay_msec, nullptr);
}

void MessagePumpForWin::runWithDispatcher(Delegate* delegate, MessagePumpDispatcher* dispatcher)
{
    RunState state;

    state.delegate    = delegate;
    state.dispatcher  = dispatcher;
    state.should_quit = false;
    state.run_depth   = state_ ? state_->run_depth + 1 : 1;

    RunState* previous_state = state_;
    state_ = &state;

    doRunLoop();

    state_ = previous_state;
}

bool MessagePumpForWin::onMessage(
    UINT message, WPARAM /* wparam */, LPARAM /* lparam */, LRESULT& /* result */)
{
    switch (message)
    {
        case kMsgHaveWork:
            handleWorkMessage();
            break;

        case WM_TIMER:
            handleTimerMessage();
            break;

        default:
            break;
    }

    return false;
}

void MessagePumpForWin::doRunLoop()
{
    // IF this was just a simple PeekMessage() loop (servicing all possible work queues), then
    // Windows would try to achieve the following order according to MSDN documentation about
    // PeekMessage with no filter):
    //    * Sent messages
    //    * Posted messages
    //    * Sent messages (again)
    //    * WM_PAINT messages
    //    * WM_TIMER messages
    //
    // Summary: none of the above classes is starved, and sent messages has twice the chance of
    // being processed (i.e., reduced service time).

    for (;;)
    {
        // If we do any work, we may create more messages etc., and more work may possibly be
        // waiting in another task group. When we (for example) processNextWindowsMessage(), there
        // is a good chance there are still more messages waiting.  On the other hand, when any of
        // these methods return having done no work, then it is pretty unlikely that calling them
        // again quickly will find any work to do. Finally, if they all say they had no work, then
        // it is a good time to consider sleeping (waiting) for more work.

        bool more_work_is_plausible = processNextWindowsMessage();
        if (state_->should_quit)
            break;

        more_work_is_plausible |= state_->delegate->doWork();
        if (state_->should_quit)
            break;

        more_work_is_plausible |= state_->delegate->doDelayedWork(&delayed_work_time_);

        // If we did not process any delayed work, then we can assume that our existing WM_TIMER if
        // any will fire when delayed work should run. We don't want to disturb that timer if it is
        // already in flight. However, if we did do all remaining delayed work, then lets kill the
        // WM_TIMER.
        if (more_work_is_plausible && delayed_work_time_ == TimePoint())
        {
            KillTimer(message_window_.hwnd(), reinterpret_cast<UINT_PTR>(this));
        }

        if (state_->should_quit)
            break;

        if (more_work_is_plausible)
            continue;

        more_work_is_plausible = state_->delegate->doIdleWork();
        if (state_->should_quit)
            break;

        if (more_work_is_plausible)
            continue;

        waitForWork(); // Wait (sleep) until we have work to do again.
    }
}

void MessagePumpForWin::waitForWork()
{
    DWORD wait_flags = MWMO_INPUTAVAILABLE;

    for (;;)
    {
        DWORD result = MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, wait_flags);
        if (result == WAIT_OBJECT_0)
        {
            // A WM_* message is available.
            // If a parent child relationship exists between windows across threads then their
            // thread inputs are implicitly attached. This causes the MsgWaitForMultipleObjectsEx
            // API to return indicating that messages are ready for processing (Specifically, mouse
            // messages intended for the child window may appear if the child window has capture).
            // The subsequent PeekMessages call may fail to return any messages thus causing us to
            // enter a tight loop at times. The code below is a workaround to give the child window
            // some time to process its input messages by looping back to MsgWaitForMultipleObjectsEx
            // above when there are no messages for the current thread.

            MSG msg = { 0 };

            bool has_pending_sent_message =
                (HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

            if (has_pending_sent_message ||
                PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE))
            {
                return;
            }

            // We know there are no more messages for this thread because PeekMessage has returned
            // false. Reset |wait_flags| so that we wait for a *new* message.
            wait_flags = 0;
        }

        DCHECK_NE(WAIT_FAILED, result) << GetLastError();
    }
}

void MessagePumpForWin::handleWorkMessage()
{
    // If we are being called outside of the context of Run, then don't try to do any work. This
    // could correspond to a MessageBox call or something of that sort.
    if (!state_)
    {
        // Since we handled a kMsgHaveWork message, we must still update this flag.
        InterlockedExchange(&work_state_, READY);
        return;
    }

    // Let whatever would have run had we not been putting messages in the queue run now. This is
    // an attempt to make our dummy message not starve other messages that may be in the Windows
    // message queue.
    processPumpReplacementMessage();

    // Now give the delegate a chance to do some work.  He'll let us know if he needs to do more
    // work.
    if (state_->delegate->doWork())
        scheduleWork();
}

void MessagePumpForWin::handleTimerMessage()
{
    KillTimer(message_window_.hwnd(), reinterpret_cast<UINT_PTR>(this));

    // If we are being called outside of the context of Run, then don't do anything. This could
    // correspond to a MessageBox call or something of that sort.
    if (!state_)
        return;

    state_->delegate->doDelayedWork(&delayed_work_time_);
    if (delayed_work_time_ != TimePoint())
    {
        // A bit gratuitous to set delayed_work_time_ again, but oh well.
        scheduleDelayedWork(delayed_work_time_);
    }
}

bool MessagePumpForWin::processNextWindowsMessage()
{
    // If there are sent messages in the queue then PeekMessage internally dispatches the message
    // and returns false. We return true in this case to ensure that the message loop peeks again
    // instead of calling MsgWaitForMultipleObjectsEx again.
    bool sent_messages_in_queue = false;

    DWORD queue_status = GetQueueStatus(QS_SENDMESSAGE);

    if (HIWORD(queue_status) & QS_SENDMESSAGE)
        sent_messages_in_queue = true;

    MSG msg;

    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
        return processMessageHelper(msg);

    return sent_messages_in_queue;
}

bool MessagePumpForWin::processMessageHelper(const MSG& msg)
{
    if (msg.message == WM_QUIT)
    {
        // Repost the QUIT message so that it will be retrieved by the primary
        // GetMessage() loop.
        state_->should_quit = true;
        PostQuitMessage(static_cast<int>(msg.wParam));
        return false;
    }

    // While running our main message pump, we discard kMsgHaveWork messages.
    if (msg.message == kMsgHaveWork && msg.hwnd == message_window_.hwnd())
        return processPumpReplacementMessage();

    if (state_->dispatcher)
    {
        if (!state_->dispatcher->dispatch(msg))
            state_->should_quit = true;
    }
    else
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

bool MessagePumpForWin::processPumpReplacementMessage()
{
    // When we encounter a kMsgHaveWork message, this method is called to peek and process a
    // replacement message, such as a WM_PAINT or WM_TIMER. The goal is to make the kMsgHaveWork as
    // non-intrusive as possible, even though a continuous stream of such messages are posted.
    // This method carefully peeks a message while there is no chance for a kMsgHaveWork to be
    // pending, then resets the have_work_ flag (allowing a replacement kMsgHaveWork to possibly be
    // posted), and finally dispatches that peeked replacement. Note that the re-post of
    // kMsgHaveWork may be asynchronous to this thread!!

    MSG msg;

    const bool have_message = (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE);

    // Expect no message or a message different than kMsgHaveWork.
    DCHECK(!have_message || kMsgHaveWork != msg.message || msg.hwnd != message_window_.hwnd());

    // Since we discarded a kMsgHaveWork message, we must update the flag.
    int old_have_work = InterlockedExchange(&work_state_, READY);
    DCHECK(old_have_work);

    // We don't need a special time slice if we didn't have_message to process.
    if (!have_message)
        return false;

    // Guarantee we'll get another time slice in the case where we go into native
    // windows code. This scheduleWork() may hurt performance a tiny bit when
    // tasks appear very infrequently, but when the event queue is busy, the
    // kMsgHaveWork events get (percentage wise) rarer and rarer.
    scheduleWork();

    return processMessageHelper(msg);
}

int MessagePumpForWin::currentDelay() const
{
    if (delayed_work_time_ == TimePoint())
        return -1;

    // Be careful here. TimeDelta has a precision of microseconds, but we want
    // a value in milliseconds. If there are 5.5ms left, should the delay be 5
    // or 6?  It should be 6 to avoid executing delayed work too early.
    int64_t timeout = std::chrono::duration_cast<Milliseconds>(
        delayed_work_time_ - Clock::now()).count();

    // If this value is negative, then we need to run delayed work soon.
    int delay = static_cast<int>(timeout);
    if (delay < 0)
        delay = 0;

    return delay;
}

} // namespace base
