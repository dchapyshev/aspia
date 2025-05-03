//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_SESSION_WATCHER_H
#define BASE_WIN_SESSION_WATCHER_H

#include "base/macros_magic.h"
#include "base/session_id.h"
#include "base/win/session_status.h"

#include <memory>

#include <qt_windows.h>

namespace base::win {

class MessageWindow;

class SessionWatcher
{
public:
    SessionWatcher();
    ~SessionWatcher();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onSessionEvent(SessionStatus status, SessionId session_id) = 0;
    };

    bool start(Delegate* delegate);
    void stop();

private:
    bool onMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    std::unique_ptr<MessageWindow> window_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SessionWatcher);
};

} // namespace base::win

#endif // BASE_WIN_SESSION_WATCHER_H
