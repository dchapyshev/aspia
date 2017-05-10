//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_io.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_pump_io.h"
#include "base/logging.h"

namespace aspia {

MessagePumpForIO::MessagePumpForIO()
{
    port_.Reset(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1));
    DCHECK(port_.IsValid());
}

void MessagePumpForIO::DoRunLoop()
{
    for (;;)
    {
        // If we do any work, we may create more messages etc., and more work may
        // possibly be waiting in another task group.  When we (for example)
        // WaitForIOCompletion(), there is a good chance there are still more
        // messages waiting.  On the other hand, when any of these methods return
        // having done no work, then it is pretty unlikely that calling them
        // again quickly will find any work to do.  Finally, if they all say they
        // had no work, then it is a good time to consider sleeping (waiting) for
        // more work.

        bool more_work_is_plausible = state_->delegate->DoWork();
        if (state_->should_quit)
            break;

        more_work_is_plausible |= WaitForIOCompletion(0, nullptr);
        if (state_->should_quit)
            break;

        if (more_work_is_plausible)
            continue;

        WaitForWork(); // Wait (sleep) until we have work to do again.
    }
}

void MessagePumpForIO::ScheduleWork()
{
    if (InterlockedExchange(&work_state_, HAVE_WORK) != READY)
        return; // Someone else continued the pumping.

    // Make sure the MessagePump does some work for us.
    BOOL ret = PostQueuedCompletionStatus(port_, 0,
                                          reinterpret_cast<ULONG_PTR>(this),
                                          reinterpret_cast<OVERLAPPED*>(this));
    if (ret)
        return; // Post worked perfectly.

    // See comment in MessagePumpUI::ScheduleWork() for this error recovery.
    InterlockedExchange(&work_state_, READY); // Clarify that we didn't succeed.
}

void MessagePumpForIO::RegisterIOHandler(HANDLE file_handle, IOHandler* handler)
{
    ULONG_PTR key = reinterpret_cast<ULONG_PTR>(handler);
    HANDLE port = CreateIoCompletionPort(file_handle, port_, key, 1);
    DCHECK(port);
}

bool MessagePumpForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter)
{
    IOItem item;

    if (completed_io_.empty() || !MatchCompletedIOItem(filter, &item))
    {
        // We have to ask the system for another IO completion.
        if (!GetIOItem(timeout, &item))
            return false;

        if (ProcessInternalIOItem(item))
            return true;
    }

    if (item.context->handler)
    {
        if (filter && item.handler != filter)
        {
            // Save this item for later
            completed_io_.push_back(item);
        }
        else
        {
            DCHECK_EQ(item.context->handler, item.handler);

            item.handler->OnIOCompleted(item.context,
                                        item.bytes_transfered,
                                        item.error);
        }
    }
    else
    {
        // The handler must be gone by now, just cleanup the mess.
        delete item.context;
    }
    return true;
}

// Wait until IO completes, up to the time needed by the timer manager to fire
// the next set of timers.
void MessagePumpForIO::WaitForWork()
{
    // We do not support nested IO message loops. This is to avoid messy
    // recursion problems.
    DCHECK_EQ(1, state_->run_depth) << "Cannot nest an IO message loop!";

    WaitForIOCompletion(INFINITE, nullptr);
}

bool MessagePumpForIO::MatchCompletedIOItem(IOHandler* filter, IOItem* item)
{
    DCHECK(!completed_io_.empty());

    for (std::list<IOItem>::iterator it = completed_io_.begin();
         it != completed_io_.end(); ++it)
    {
        if (!filter || it->handler == filter)
        {
            *item = *it;
            completed_io_.erase(it);
            return true;
        }
    }

    return false;
}

bool MessagePumpForIO::GetIOItem(DWORD timeout, IOItem* item)
{
    memset(item, 0, sizeof(*item));

    ULONG_PTR key = 0;
    OVERLAPPED* overlapped = nullptr;

    if (!GetQueuedCompletionStatus(port_.Get(),
                                   &item->bytes_transfered,
                                   &key,
                                   &overlapped,
                                   timeout))
    {
        if (!overlapped)
            return false; // Nothing in the queue.

        item->error = GetLastError();
        item->bytes_transfered = 0;
    }

    item->handler = reinterpret_cast<IOHandler*>(key);
    item->context = reinterpret_cast<IOContext*>(overlapped);

    return true;
}


bool MessagePumpForIO::ProcessInternalIOItem(const IOItem& item)
{
    if (this == reinterpret_cast<MessagePumpForIO*>(item.context) &&
        this == reinterpret_cast<MessagePumpForIO*>(item.handler))
    {
        // This is our internal completion.
        DCHECK(!item.bytes_transfered);
        InterlockedExchange(&work_state_, READY);
        return true;
    }

    return false;
}

} // namespace aspia
