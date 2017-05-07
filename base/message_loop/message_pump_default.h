//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_default.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H

#include "base/message_loop/message_pump.h"
#include "base/macros.h"

#include <condition_variable>

namespace aspia {

class MessagePumpDefault : public MessagePump
{
public:
    MessagePumpDefault();
    virtual ~MessagePumpDefault() {}

    // MessagePump methods:
    virtual void Run(Delegate* delegate) override;
    virtual void Quit() override;
    virtual void ScheduleWork() override;

private:
    // This flag is set to false when Run should return.
    bool keep_running_;

    // Used to sleep until there is more work to do.
    std::condition_variable event_;

    bool have_work_;
    std::mutex have_work_lock_;

    DISALLOW_COPY_AND_ASSIGN(MessagePumpDefault);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H
