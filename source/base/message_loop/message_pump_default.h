//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_pump_default.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H

#include <QtGlobal>
#include <condition_variable>
#include <mutex>

#include "base/message_loop/message_pump.h"

namespace aspia {

class MessagePumpDefault : public MessagePump
{
public:
    MessagePumpDefault() = default;
    ~MessagePumpDefault() = default;

    // MessagePump methods:
    void Run(Delegate* delegate) override;
    void Quit() override;
    void ScheduleWork() override;
    void ScheduleDelayedWork(
        const std::chrono::time_point<std::chrono::high_resolution_clock>& delayed_work_time) override;

private:
    // This flag is set to false when Run should return.
    bool keep_running_ = true;

    // Used to sleep until there is more work to do.
    std::condition_variable event_;

    bool have_work_ = false;
    std::mutex have_work_lock_;

    // The time at which we should call DoDelayedWork.
    std::chrono::time_point<std::chrono::high_resolution_clock> delayed_work_time_;

    Q_DISABLE_COPY(MessagePumpDefault)
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H
