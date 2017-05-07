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
    keep_running_(true),
    have_work_(false)
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

        std::unique_lock<std::mutex> lock(have_work_lock_);

        while (!have_work_)
            event_.wait(lock);

        have_work_ = false;
    }

    keep_running_ = true;
}

void MessagePumpDefault::Quit()
{
    keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork()
{
    {
        std::unique_lock<std::mutex> lock(have_work_lock_);
        have_work_ = true;
    }

    // Since this can be called on any thread, we need to ensure that our Run
    // loop wakes up.
    event_.notify_one();
}

}  // namespace base
