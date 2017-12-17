//
// PROJECT:         Aspia
// FILE:            host/console_session_watcher.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/console_session_watcher.h"
#include "base/files/base_paths.h"
#include "base/logging.h"

#include <wtsapi32.h>

namespace aspia {

const wchar_t kWtsApi32LibraryName[] = L"wtsapi32.dll";

ConsoleSessionWatcher::ConsoleSessionWatcher()
{
    FilePath library_path;

    if (GetBasePath(BasePathKey::DIR_SYSTEM, library_path))
    {
        library_path.append(kWtsApi32LibraryName);

        wtsapi32_library_ = std::make_unique<ScopedNativeLibrary>(library_path.c_str());

        if (wtsapi32_library_->IsLoaded())
        {
            register_session_notification_ =
                reinterpret_cast<WTSRegisterSessionNotificationFn>(
                    wtsapi32_library_->GetFunctionPointer("WTSRegisterSessionNotification"));

            unregister_session_notification_ =
                reinterpret_cast<WTSUnRegisterSessionNotificationFn>(
                    wtsapi32_library_->GetFunctionPointer("WTSUnRegisterSessionNotification"));
        }
    }

    CHECK(register_session_notification_ && unregister_session_notification_);
}

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

    if (!register_session_notification_(window_->hwnd(), NOTIFY_FOR_ALL_SESSIONS))
    {
        LOG(ERROR) << "WTSRegisterSessionNotification() failed: "
                   << GetLastSystemErrorString();
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
