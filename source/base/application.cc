//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/application.h"

#include "base/logging.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <wtsapi32.h>
#include "base/session_id.h"
#include "base/win/message_window.h"
#include "base/win/session_status.h"
#endif // defined(Q_OS_WINDOWS)

namespace base {

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : QCoreApplication(argc, argv),
#if defined(Q_OS_WINDOWS)
      ui_thread_(base::Thread::QtDispatcher)
#endif // defined(Q_OS_WINDOWS)
{
#if defined(Q_OS_WINDOWS)
    auto on_window_message =
        [this](UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result) -> bool
    {
        switch (message)
        {
            case WM_WTSSESSION_CHANGE:
            {
                if (!is_service_) // Service emits these signals.
                {
                    quint32 event = static_cast<quint32>(wparam);
                    quint32 session_id = static_cast<quint32>(lparam);

                    LOG(INFO) << "WM_WTSSESSION_CHANGE received (event:" << sessionStatusToString(event)
                              << "session id:" << session_id << ")";
                    emit sig_sessionEvent(event, session_id);
                }

                result = TRUE;
                return true;
            }

            case WM_POWERBROADCAST:
            {
                if (!is_service_) // Service emits these signals.
                {
                    quint32 event = static_cast<quint32>(wparam);

                    LOG(INFO) << "WM_POWERBROADCAST received (event:" << event << ")";
                    emit sig_powerEvent(event);
                }

                result = TRUE;
                return true;
            }

            case WM_QUERYENDSESSION:
            {
                LOG(INFO) << "WM_QUERYENDSESSION received";
                emit sig_queryEndSession();

                result = TRUE;
                return true;
            }

            case WM_ENDSESSION:
            {
                LOG(INFO) << "WM_ENDSESSION received";
                result = FALSE;
                return true;
            }

            default:
                break;
        }

        return false;
    };

    is_service_ = base::currentProcessSessionId() == 0;

    connect(&ui_thread_, &base::Thread::started, this, [this, on_window_message]()
    {
        LOG(INFO) << "UI thread starting";

        if (!is_service_)
        {
            LOG(INFO) << "Application mode detected";

            if (!WTSRegisterSessionNotification(message_window_->hwnd(), NOTIFY_FOR_ALL_SESSIONS))
            {
                PLOG(ERROR) << "WTSRegisterSessionNotification failed";
            }
        }
        else
        {
            LOG(INFO) << "Service mode detected";
        }

        message_window_ = std::make_unique<base::MessageWindow>();
        if (!message_window_->create(on_window_message))
        {
            LOG(ERROR) << "Couldn't create window";
            return;
        }
    },
    Qt::DirectConnection);

    connect(&ui_thread_, &base::Thread::finished, this, [this]()
    {
        LOG(INFO) << "UI thread stopping";

        if (!WTSUnRegisterSessionNotification(message_window_->hwnd()))
        {
            PLOG(ERROR) << "WTSUnRegisterSessionNotification failed";
        }

        message_window_.reset();
    },
    Qt::DirectConnection);

    ui_thread_.start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
Application::~Application()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    ui_thread_.stop();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
// static
Application* Application::instance()
{
    return static_cast<Application*>(QCoreApplication::instance());
}

} // namespace base
