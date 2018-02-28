//
// PROJECT:         Aspia
// FILE:            host/console_session_watcher.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/console_session_watcher.h"

#include <wtsapi32.h>

#include "base/files/base_paths.h"
#include "base/logging.h"

namespace aspia {

ConsoleSessionWatcher::~ConsoleSessionWatcher()
{
    StopWatching();
}

bool ConsoleSessionWatcher::StartWatching(Delegate* delegate)
{
    delegate_ = delegate;

    window_ = std::make_unique<MessageWindow>();

    if (!window_->Create(std::bind(&ConsoleSessionWatcher::OnMessage,
                                   this,
                                   std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3, std::placeholders::_4)))
    {
        return false;
    }

    if (!WTSRegisterSessionNotification(window_->hwnd(), NOTIFY_FOR_ALL_SESSIONS))
    {
        PLOG(LS_ERROR) << "WTSRegisterSessionNotification failed";
        window_.reset();
        return false;
    }

    return true;
}

void ConsoleSessionWatcher::StopWatching()
{
    if (window_)
    {
        WTSUnRegisterSessionNotification(window_->hwnd());
        window_.reset();
        delegate_ = nullptr;
    }
}

bool ConsoleSessionWatcher::OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    if (message == WM_WTSSESSION_CHANGE)
    {
        // Process only attach/detach notifications.
        switch (wParam)
        {
            case WTS_CONSOLE_CONNECT:
                delegate_->OnSessionAttached(static_cast<uint32_t>(lParam));
                break;

            case WTS_CONSOLE_DISCONNECT:
                delegate_->OnSessionDetached();
                break;
        }

        result = 0;
        return true;
    }

    return false;
}

} // namespace aspia
