//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_watcher.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/console_session_watcher.h"
#include "base/logging.h"

#include <wtsapi32.h>

namespace aspia {

ConsoleSessionWatcher::~ConsoleSessionWatcher()
{
    StopWatching();
}

bool ConsoleSessionWatcher::StartWatching(Delegate* delegate)
{
    delegate_ = delegate;

    window_.reset(new MessageWindow());

    if (!window_->Create(std::bind(&ConsoleSessionWatcher::OnMessage,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2,
                                   std::placeholders::_3,
                                   std::placeholders::_4)))
    {
        return false;
    }

    if (!WTSRegisterSessionNotificationEx(WTS_CURRENT_SERVER,
                                          window_->hwnd(),
                                          NOTIFY_FOR_ALL_SESSIONS))
    {
        LOG(ERROR) << "WTSRegisterSessionNotificationEx() failed: " << GetLastError();
        return false;
    }

    return true;
}

void ConsoleSessionWatcher::StopWatching()
{
    if (window_)
    {
        WTSUnRegisterSessionNotificationEx(WTS_CURRENT_SERVER, window_->hwnd());
        window_.reset();
        delegate_ = nullptr;
    }
}

bool ConsoleSessionWatcher::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    if (message == WM_WTSSESSION_CHANGE)
    {
        // Process only attach/detach notifications.
        switch (wParam)
        {
            case WTS_CONSOLE_CONNECT:
                delegate_->OnSessionAttached(lParam);
                break;

            case WTS_CONSOLE_DISCONNECT:
                delegate_->OnSessionDetached();
                break;
        }

        *result = 0;
        return true;
    }

    return false;
}

} // namespace aspia
