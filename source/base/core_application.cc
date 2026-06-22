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

#include "base/core_application.h"

#include "base/logging.h"
#include "base/session_id.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <wtsapi32.h>
#include "base/thread.h"
#include "base/win/message_window.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QSocketNotifier>
#include "base/linux/libsystemd.h"
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
CoreApplication::CoreApplication(int& argc, char* argv[])
    : QCoreApplication(argc, argv)
{
#if defined(Q_OS_WINDOWS)
    ui_thread_ = std::make_unique<Thread>(Thread::QtDispatcher);

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

    is_service_ = currentProcessSessionId() == 0;

    connect(ui_thread_.get(), &Thread::sig_beforeRunning, this, [this, on_window_message]()
    {
        LOG(INFO) << "UI thread starting";

        message_window_ = std::make_unique<MessageWindow>();
        if (!message_window_->create(on_window_message))
        {
            LOG(ERROR) << "Couldn't create window";
            return;
        }

        if (!is_service_)
        {
            LOG(INFO) << "CoreApplication mode detected";

            if (!WTSRegisterSessionNotification(message_window_->hwnd(), NOTIFY_FOR_ALL_SESSIONS))
                PLOG(ERROR) << "WTSRegisterSessionNotification failed";
        }
        else
        {
            LOG(INFO) << "Service mode detected";
        }
    },
    Qt::DirectConnection);

    connect(ui_thread_.get(), &Thread::sig_afterRunning, this, [this]()
    {
        LOG(INFO) << "UI thread stopping";

        if (message_window_ && !is_service_)
        {
            if (!WTSUnRegisterSessionNotification(message_window_->hwnd()))
                PLOG(ERROR) << "WTSUnRegisterSessionNotification failed";
        }

        message_window_.reset();
    },
    Qt::DirectConnection);

    ui_thread_->start();
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Watch the active session on the local seat via logind and report changes, so the service can
    // follow the console session (user switching, display-manager greeter) like the WTS session
    // notifications on Windows.
    if (LibSystemd::loginMonitorNew("session", &login_monitor_) >= 0 && login_monitor_)
    {
        last_active_session_ = activeConsoleSessionId();

        session_notifier_ = new QSocketNotifier(
            LibSystemd::loginMonitorGetFd(login_monitor_), QSocketNotifier::Read);

        connect(session_notifier_, &QSocketNotifier::activated, this, [this]()
        {
            LibSystemd::loginMonitorFlush(login_monitor_);

            SessionId active = activeConsoleSessionId();
            if (active == last_active_session_)
                return;

            LOG(INFO) << "Active console session changed:" << last_active_session_ << "->" << active;
            last_active_session_ = active;
            emit sig_sessionEvent(0, static_cast<quint32>(active));
        });
    }
    else
    {
        LOG(ERROR) << "sd_login_monitor_new failed";
    }
#endif // defined(Q_OS_LINUX)
}

//--------------------------------------------------------------------------------------------------
CoreApplication::~CoreApplication()
{
    LOG(INFO) << "Dtor";

#if defined(Q_OS_WINDOWS)
    if (ui_thread_)
        ui_thread_->stop();
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (login_monitor_)
        login_monitor_ = LibSystemd::loginMonitorUnref(login_monitor_);
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
}

//--------------------------------------------------------------------------------------------------
// static
CoreApplication* CoreApplication::instance()
{
    return static_cast<CoreApplication*>(QCoreApplication::instance());
}
