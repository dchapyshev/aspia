//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_dispatcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_DISPATCHER_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_DISPATCHER_H

#include "base/message_loop/message_loop.h"

namespace aspia {

template <bool kIsDialogDispatcher>
class MessageDispatcher : public MessageLoopForUI::Dispatcher
{
public:
    MessageDispatcher(HWND hwnd) :
        hwnd_(hwnd)
    {
        // Nothing
    }

private:
    bool Dispatch(const NativeEvent& event) override
    {
        if (kIsDialogDispatcher)
        {
            if (IsDialogMessageW(hwnd_, const_cast<MSG*>(&event)))
                return true;
        }

        TranslateMessage(&event);
        DispatchMessageW(&event);

        return true;
    }

    HWND hwnd_;

    DISALLOW_COPY_AND_ASSIGN(MessageDispatcher);
};

using MessageDispatcherForDialog = MessageDispatcher<true>;
using MessageDispatcherForWindow = MessageDispatcher<false>;

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_DISPATCHER_H
