//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/message_dispatcher.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__MESSAGE_DISPATCHER_H
#define _ASPIA_UI__BASE__MESSAGE_DISPATCHER_H

#include "base/message_loop/message_loop.h"

namespace aspia {

template <bool kIsDialogDispatcher>
class UiMessageDispatcher : public MessageLoopForUI::Dispatcher
{
public:
    UiMessageDispatcher(HWND hwnd) :
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

    DISALLOW_COPY_AND_ASSIGN(UiMessageDispatcher);
};

using UiMessageDispatcherForDialog = UiMessageDispatcher<true>;
using UiMessageDispatcherForWindow = UiMessageDispatcher<false>;

} // namespace aspia

#endif // _ASPIA_UI__BASE__MESSAGE_DISPATCHER_H
