//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_watcher.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/console_session_watcher.h"

#include <wtsapi32.h>

namespace aspia {

ConsoleSessionWatcher::ConsoleSessionWatcher()
{
    // Nothing
}

ConsoleSessionWatcher::~ConsoleSessionWatcher()
{
    DestroyMessageWindow();
}

void ConsoleSessionWatcher::StartSessionWatching()
{
    CreateMessageWindow();
}

void ConsoleSessionWatcher::StopSessionWatching()
{
    DestroyMessageWindow();
}

void ConsoleSessionWatcher::OnMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_CREATE:
        {
            if (!WTSRegisterSessionNotificationEx(WTS_CURRENT_SERVER,
                                                  hwnd,
                                                  NOTIFY_FOR_ALL_SESSIONS))
            {
                LOG(ERROR) << "WTSRegisterSessionNotificationEx() failed: " << GetLastError();
            }

            PostMessageW(hwnd,
                         WM_WTSSESSION_CHANGE,
                         WTS_CONSOLE_CONNECT,
                         WTSGetActiveConsoleSessionId());
        }
        break;

        case WM_DESTROY:
        {
            WTSUnRegisterSessionNotificationEx(WTS_CURRENT_SERVER, hwnd);
            OnSessionDetached();
        }
        break;

        case WM_WTSSESSION_CHANGE:
        {
            // Process only attach/detach notifications.
            switch (wParam)
            {
                case WTS_CONSOLE_CONNECT:
                    OnSessionAttached(lParam);
                    break;

                case WTS_CONSOLE_DISCONNECT:
                    OnSessionDetached();
                    break;
            }
        }
        break;
    }
}

void ConsoleSessionWatcher::DetachSession()
{
    PostMessageW(GetMessageWindowHandle(),
                 WM_WTSSESSION_CHANGE,
                 WTS_CONSOLE_DISCONNECT,
                 0);
}

} // namespace aspia
