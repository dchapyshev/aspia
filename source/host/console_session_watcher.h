//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/console_session_watcher.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
#define _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H

#include <functional>
#include <memory>

#include "base/scoped_native_library.h"
#include "base/message_window.h"

namespace aspia {

class ConsoleSessionWatcher
{
public:
    ConsoleSessionWatcher();
    ~ConsoleSessionWatcher();

    class Delegate
    {
    public:
        virtual void OnSessionDetached() = 0;
        virtual void OnSessionAttached(uint32_t session_id) = 0;
    };

    // Starts monitoring changes to the current console session.
    // Returns true if tracking could be started and false if failed.
    bool StartWatching(Delegate* delegate);

    // Stops monitoring.
    void StopWatching();

private:
    bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    typedef BOOL(WINAPI *WTSRegisterSessionNotificationFn)(HWND, DWORD);
    typedef BOOL(WINAPI *WTSUnRegisterSessionNotificationFn)(HWND);

    ScopedNativeLibrary wtsapi32_library_;
    WTSRegisterSessionNotificationFn register_session_notification_ = nullptr;
    WTSUnRegisterSessionNotificationFn unregister_session_notification_ = nullptr;

    std::unique_ptr<MessageWindow> window_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ConsoleSessionWatcher);
};

} // namespace aspia

#endif // _ASPIA_HOST__CONSOLE_SESSION_WATCHER_H
