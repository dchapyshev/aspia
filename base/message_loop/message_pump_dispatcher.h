//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_dispatcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H

namespace aspia {

using NativeEvent = MSG;

// Dispatcher is used during a nested invocation of Run to dispatch events when
// |RunLoop(dispatcher).Run()| is used.  If |RunLoop().Run()| is invoked,
// MessageLoop does not dispatch events (or invoke TranslateMessage), rather
// every message is passed to Dispatcher's Dispatch method for dispatch. It is
// up to the Dispatcher whether or not to dispatch the event.
//
// The nested loop is exited by either posting a quit, or returning false
// from Dispatch.
class MessagePumpDispatcher
{
public:
    virtual ~MessagePumpDispatcher() = default;

    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool Dispatch(const NativeEvent& event) = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DISPATCHER_H
