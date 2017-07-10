//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_ui.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_pump_ui.h"
#include "base/logging.h"

namespace aspia {

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const UINT kMsgHaveWork = WM_USER + 1;

MessagePumpForUI::MessagePumpForUI()
{
    bool succeeded = message_window_.Create(std::bind(&MessagePumpForUI::OnMessage,
                                                      this,
                                                      std::placeholders::_1,
                                                      std::placeholders::_2,
                                                      std::placeholders::_3,
                                                      std::placeholders::_4));
    DCHECK(succeeded);
}

void MessagePumpForUI::ScheduleWork()
{
    if (InterlockedExchange(&work_state_, HAVE_WORK) != READY)
        return; // Someone else continued the pumping.

    // Make sure the MessagePump does some work for us.
    BOOL ret = PostMessageW(message_window_.hwnd(), kMsgHaveWork, 0, 0);
    if (ret)
        return; // There was room in the Window Message queue.

    // We have failed to insert a have-work message, so there is a chance that we
    // will starve tasks/timers while sitting in a nested message loop.  Nested
    // loops only look at Windows Message queues, and don't look at *our* task
    // queues, etc., so we might not get a time slice in such. :-(
    // We could abort here, but the fear is that this failure mode is plausibly
    // common (queue is full, of about 2000 messages), so we'll do a near-graceful
    // recovery.  Nested loops are pretty transient (we think), so this will
    // probably be recoverable.
    InterlockedExchange(&work_state_, READY); // Clarify that we didn't really insert.
}

bool MessagePumpForUI::OnMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result)
{
    UNREF(wparam);
    UNREF(lparam);
    UNREF(result);

    switch (message)
    {
        case kMsgHaveWork:
            HandleWorkMessage();
            break;
    }

    return false;
}

void MessagePumpForUI::DoRunLoop()
{
    // IF this was just a simple PeekMessage() loop (servicing all possible work
    // queues), then Windows would try to achieve the following order according
    // to MSDN documentation about PeekMessage with no filter):
    //    * Sent messages
    //    * Posted messages
    //    * Sent messages (again)
    //    * WM_PAINT messages
    //    * WM_TIMER messages
    //
    // Summary: none of the above classes is starved, and sent messages has twice
    // the chance of being processed (i.e., reduced service time).

    for (;;)
    {
        // If we do any work, we may create more messages etc., and more work may
        // possibly be waiting in another task group.  When we (for example)
        // ProcessNextWindowsMessage(), there is a good chance there are still more
        // messages waiting.  On the other hand, when any of these methods return
        // having done no work, then it is pretty unlikely that calling them again
        // quickly will find any work to do.  Finally, if they all say they had no
        // work, then it is a good time to consider sleeping (waiting) for more
        // work.

        bool more_work_is_plausible = ProcessNextWindowsMessage();
        if (state_->should_quit)
            break;

        more_work_is_plausible |= state_->delegate->DoWork();
        if (state_->should_quit)
            break;

        if (more_work_is_plausible)
            continue;

        WaitForWork(); // Wait (sleep) until we have work to do again.
    }
}

void MessagePumpForUI::WaitForWork()
{
    DWORD wait_flags = MWMO_INPUTAVAILABLE;

    for (;;)
    {
        DWORD result = MsgWaitForMultipleObjectsEx(0,
                                                   nullptr,
                                                   INFINITE,
                                                   QS_ALLINPUT,
                                                   wait_flags);
        if (result == WAIT_OBJECT_0)
        {
            // A WM_* message is available.
            // If a parent child relationship exists between windows across threads
            // then their thread inputs are implicitly attached.
            // This causes the MsgWaitForMultipleObjectsEx API to return indicating
            // that messages are ready for processing (Specifically, mouse messages
            // intended for the child window may appear if the child window has
            // capture).
            // The subsequent PeekMessages call may fail to return any messages thus
            // causing us to enter a tight loop at times.
            // The code below is a workaround to give the child window
            // some time to process its input messages by looping back to
            // MsgWaitForMultipleObjectsEx above when there are no messages for the
            // current thread.

            MSG msg = { 0 };

            bool has_pending_sent_message =
                (HIWORD(GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

            if (has_pending_sent_message ||
                PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE))
            {
                return;
            }

            // We know there are no more messages for this thread because PeekMessage
            // has returned false. Reset |wait_flags| so that we wait for a *new*
            // message.
            wait_flags = 0;
        }

        DCHECK_NE(WAIT_FAILED, result) << GetLastSystemErrorString();
    }
}

void MessagePumpForUI::HandleWorkMessage()
{
    // If we are being called outside of the context of Run, then don't try to do
    // any work. This could correspond to a MessageBox call or something of that
    // sort.
    if (!state_)
    {
        // Since we handled a kMsgHaveWork message, we must still update this flag.
        InterlockedExchange(&work_state_, READY);
        return;
    }

    // Let whatever would have run had we not been putting messages in the queue
    // run now. This is an attempt to make our dummy message not starve other
    // messages that may be in the Windows message queue.
    ProcessPumpReplacementMessage();

    // Now give the delegate a chance to do some work.  He'll let us know if he
    // needs to do more work.
    if (state_->delegate->DoWork())
        ScheduleWork();
}

bool MessagePumpForUI::ProcessNextWindowsMessage()
{
    // If there are sent messages in the queue then PeekMessage internally
    // dispatches the message and returns false. We return true in this
    // case to ensure that the message loop peeks again instead of calling
    // MsgWaitForMultipleObjectsEx again.
    bool sent_messages_in_queue = false;

    DWORD queue_status = GetQueueStatus(QS_SENDMESSAGE);

    if (HIWORD(queue_status) & QS_SENDMESSAGE)
        sent_messages_in_queue = true;

    MSG msg;

    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
        return ProcessMessageHelper(msg);

    return sent_messages_in_queue;
}

bool MessagePumpForUI::ProcessMessageHelper(const MSG& msg)
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
        return ProcessPumpReplacementMessage();

    if (state_->dispatcher)
    {
        if (!state_->dispatcher->Dispatch(msg))
            state_->should_quit = true;
    }
    else
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

bool MessagePumpForUI::ProcessPumpReplacementMessage()
{
    // When we encounter a kMsgHaveWork message, this method is called to peek
    // and process a replacement message, such as a WM_PAINT or WM_TIMER.  The
    // goal is to make the kMsgHaveWork as non-intrusive as possible, even though
    // a continuous stream of such messages are posted.  This method carefully
    // peeks a message while there is no chance for a kMsgHaveWork to be pending,
    // then resets the have_work_ flag (allowing a replacement kMsgHaveWork to
    // possibly be posted), and finally dispatches that peeked replacement.  Note
    // that the re-post of kMsgHaveWork may be asynchronous to this thread!!

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
    // windows code. This ScheduleWork() may hurt performance a tiny bit when
    // tasks appear very infrequently, but when the event queue is busy, the
    // kMsgHaveWork events get (percentage wise) rarer and rarer.
    ScheduleWork();

    return ProcessMessageHelper(msg);
}

} // namespace aspia
