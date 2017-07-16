//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H

namespace aspia {

class MessagePump
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual bool DoWork() = 0;
    };

    virtual ~MessagePump() = default;

    virtual void Run(Delegate* delegate) = 0;
    virtual void Quit() = 0;
    virtual void ScheduleWork() = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
