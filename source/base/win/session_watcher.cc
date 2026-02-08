//
// SmartCafe Project
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

#include "base/win/session_watcher.h"

#include "base/logging.h"
#include "base/win/message_window.h"

#include <WtsApi32.h>

namespace base {

//--------------------------------------------------------------------------------------------------
SessionWatcher::SessionWatcher() = default;

//--------------------------------------------------------------------------------------------------
SessionWatcher::~SessionWatcher()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
bool SessionWatcher::start()
{
    if (window_)
        return false;

    std::unique_ptr<MessageWindow> window = std::make_unique<MessageWindow>();

    if (!window->create(std::bind(&SessionWatcher::onMessage,
                                  this,
                                  std::placeholders::_1, std::placeholders::_2,
                                  std::placeholders::_3, std::placeholders::_4)))
    {
        LOG(ERROR) << "Unable to create window";
        return false;
    }

    if (!WTSRegisterSessionNotification(window->hwnd(), NOTIFY_FOR_ALL_SESSIONS))
    {
        PLOG(ERROR) << "WTSRegisterSessionNotification failed";
        return false;
    }

    window_ = std::move(window);
    return true;
}

//--------------------------------------------------------------------------------------------------
void SessionWatcher::stop()
{
    if (!window_)
        return;

    if (!WTSUnRegisterSessionNotification(window_->hwnd()))
    {
        PLOG(ERROR) << "WTSUnRegisterSessionNotification failed";
    }

    window_.reset();
}

//--------------------------------------------------------------------------------------------------
bool SessionWatcher::onMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    if (message == WM_WTSSESSION_CHANGE)
    {
        emit sig_sessionEvent(
            static_cast<SessionStatus>(wParam), static_cast<SessionId>(lParam));

        result = 0;
        return true;
    }

    return false;
}

} // namespace base
