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

const wchar_t kWtsApi32LibraryName[] = L"wtsapi32.dll";

ConsoleSessionWatcher::ConsoleSessionWatcher() :
    wtsapi32_library_(kWtsApi32LibraryName)
{
    register_session_notification_ =
        reinterpret_cast<WTSRegisterSessionNotificationFn>(
            wtsapi32_library_.GetFunctionPointer("WTSRegisterSessionNotification"));

    unregister_session_notification_ =
        reinterpret_cast<WTSUnRegisterSessionNotificationFn>(
            wtsapi32_library_.GetFunctionPointer("WTSUnRegisterSessionNotification"));

    CHECK(register_session_notification_ && unregister_session_notification_);
}

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

    if (!register_session_notification_(window_->hwnd(),
                                        NOTIFY_FOR_ALL_SESSIONS))
    {
        LOG(ERROR) << "WTSRegisterSessionNotification() failed: " << GetLastError();
        window_.reset();
        return false;
    }

    return true;
}

void ConsoleSessionWatcher::StopWatching()
{
    if (window_)
    {
        unregister_session_notification_(window_->hwnd());
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
