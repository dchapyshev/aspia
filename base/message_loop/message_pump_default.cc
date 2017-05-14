//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_default.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_pump_default.h"
#include "base/logging.h"

namespace aspia {

MessagePumpDefault::MessagePumpDefault() :
    event_(WaitableEvent::ResetPolicy::AUTOMATIC,
           WaitableEvent::InitialState::NOT_SIGNALED)
{
    // Nothing
}

void MessagePumpDefault::Run(Delegate* delegate)
{
    DCHECK(keep_running_) << "Quit must have been called outside of Run!";

    for (;;)
    {
        bool did_work = delegate->DoWork();

        if (!keep_running_)
            break;

        if (did_work)
            continue;

        event_.Wait();
    }

    keep_running_ = true;
}

void MessagePumpDefault::Quit()
{
    keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork()
{
    // Since this can be called on any thread, we need to ensure that our Run
    // loop wakes up.
    event_.Signal();
}

}  // namespace base
