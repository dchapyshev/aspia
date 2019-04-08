//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/win/session_watcher.h"
#include "base/logging.h"

#include <windows.h>
#include <wtsapi32.h>

namespace base::win {

SessionWatcher::SessionWatcher(QWidget* parent)
    : QWidget(parent)
{
    // Nothing
}

void SessionWatcher::start()
{
    HWND native_window = reinterpret_cast<HWND>(winId());
    if (!native_window)
    {
        LOG(LS_ERROR) << "Unable to get native window";
        return;
    }

    if (!WTSRegisterSessionNotification(native_window, NOTIFY_FOR_ALL_SESSIONS))
    {
        PLOG(LS_ERROR) << "WTSRegisterSessionNotification failed";
        return;
    }
}

void SessionWatcher::stop()
{
    HWND native_window = reinterpret_cast<HWND>(winId());
    if (!native_window)
    {
        LOG(LS_ERROR) << "Unable to get native window";
        return;
    }

    if (!WTSUnRegisterSessionNotification(native_window))
    {
        PLOG(LS_ERROR) << "WTSUnRegisterSessionNotification failed";
        return;
    }
}

bool SessionWatcher::nativeEvent(const QByteArray& /* event_type */, void* message, long* result)
{
    MSG* native_message = reinterpret_cast<MSG*>(message);
    if (!native_message)
        return false;

    if (native_message->message != WM_WTSSESSION_CHANGE)
        return false;

    emit sessionEvent(static_cast<SessionStatus>(native_message->wParam),
                      static_cast<SessionId>(native_message->lParam));

    *result = 0;
    return true;
}

} // namespace base::win
