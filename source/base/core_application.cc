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
#include "base/threading/thread.h"
#include "base/win/message_window.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QSocketNotifier>
#include "base/linux/libsystemd.h"
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_MACOS)
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <dispatch/dispatch.h>
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_MACOS)
// Reports console session changes (fast user switching, login-screen) and power transitions
// (sleep/wake) of the local machine; each callback hops back to the application thread
// instead of requiring a CFRunLoop.
class EventMonitor
{
public:
    EventMonitor();
    ~EventMonitor();

private:
    static void sessionStoreCallback(SCDynamicStoreRef store, CFArrayRef changed_keys, void* info);
    static void powerCallback(
        void* refcon, io_service_t service, natural_t message_type, void* message_arg);

    dispatch_queue_t queue_ = nullptr;

    // Console session (fast user switching, login-screen) via SCDynamicStore.
    SCDynamicStoreRef session_store_ = nullptr;
    SessionId last_active_session_ = kInvalidSessionId;

    // Sleep/wake via IOKit.
    io_connect_t power_root_port_ = MACH_PORT_NULL;
    IONotificationPortRef power_notify_port_ = nullptr;
    io_object_t power_notifier_ = IO_OBJECT_NULL;

    Q_DISABLE_COPY_MOVE(EventMonitor)
};

//--------------------------------------------------------------------------------------------------
EventMonitor::EventMonitor()
    : last_active_session_(activeConsoleSessionId())
{
    queue_ = dispatch_queue_create("org.aspia.event-monitor", DISPATCH_QUEUE_SERIAL);

    SCDynamicStoreContext context = {};
    context.info = this;

    session_store_ =
        SCDynamicStoreCreate(nullptr, CFSTR("org.aspia.host"), sessionStoreCallback, &context);
    if (session_store_)
    {
        CFStringRef key = SCDynamicStoreKeyCreateConsoleUser(nullptr);
        CFArrayRef keys = CFArrayCreate(
            nullptr, reinterpret_cast<const void**>(&key), 1, &kCFTypeArrayCallBacks);

        SCDynamicStoreSetNotificationKeys(session_store_, keys, nullptr);
        SCDynamicStoreSetDispatchQueue(session_store_, queue_);

        CFRelease(keys);
        CFRelease(key);
    }
    else
    {
        LOG(ERROR) << "SCDynamicStoreCreate failed";
    }

    power_root_port_ =
        IORegisterForSystemPower(this, &power_notify_port_, powerCallback, &power_notifier_);
    if (power_root_port_ != MACH_PORT_NULL)
        IONotificationPortSetDispatchQueue(power_notify_port_, queue_);
    else
        LOG(ERROR) << "IORegisterForSystemPower failed";
}

//--------------------------------------------------------------------------------------------------
EventMonitor::~EventMonitor()
{
    // Detach both notification sources from the queue first, then drain it synchronously: a
    // callback already enqueued finishes with a valid |this| before the barrier returns, and
    // nothing new is scheduled after it, so the releases below cannot race with a callback.
    if (session_store_)
        SCDynamicStoreSetDispatchQueue(session_store_, nullptr);

    if (power_notify_port_)
        IONotificationPortSetDispatchQueue(power_notify_port_, nullptr);

    if (queue_)
    {
        dispatch_sync_f(queue_, nullptr, [](void* /* context */) {});
        dispatch_release(queue_);
    }

    if (session_store_)
        CFRelease(session_store_);

    if (power_notify_port_)
    {
        IODeregisterForSystemPower(&power_notifier_);
        IOServiceClose(power_root_port_);
        IONotificationPortDestroy(power_notify_port_);
    }
}

//--------------------------------------------------------------------------------------------------
// static
void EventMonitor::sessionStoreCallback(
    SCDynamicStoreRef /* store */, CFArrayRef /* changed_keys */, void* info)
{
    EventMonitor* self = static_cast<EventMonitor*>(info);

    // Runs on the serial notification queue. The instance can already be gone if a notification races
    // with application shutdown.
    CoreApplication* application = CoreApplication::instance();
    if (!application)
        return;

    SessionId active = activeConsoleSessionId();
    if (active == self->last_active_session_)
        return;

    LOG(INFO) << "Active console session changed:" << self->last_active_session_ << "->" << active;
    self->last_active_session_ = active;

    // The auto/queued connections marshal the slots to their own threads, as on Windows where the emit
    // happens on the UI thread.
    emit application->sig_sessionEvent(0, static_cast<quint32>(active));
}

//--------------------------------------------------------------------------------------------------
// static
void EventMonitor::powerCallback(
    void* refcon, io_service_t /* service */, natural_t message_type, void* message_arg)
{
    EventMonitor* self = static_cast<EventMonitor*>(refcon);

    // The instance can already be gone if a notification races with application shutdown; the power
    // change is still acknowledged so the system does not stall.
    CoreApplication* application = CoreApplication::instance();

    switch (message_type)
    {
        case kIOMessageCanSystemSleep:
            // Do not veto idle sleep. The acknowledgement is mandatory, otherwise the system waits
            // 30 seconds before sleeping.
            IOAllowPowerChange(self->power_root_port_, reinterpret_cast<long>(message_arg));
            break;

        case kIOMessageSystemWillSleep:
            if (application)
                emit application->sig_powerEvent(kIOMessageSystemWillSleep);
            // Acknowledge so the system proceeds to sleep without waiting for the timeout.
            IOAllowPowerChange(self->power_root_port_, reinterpret_cast<long>(message_arg));
            break;

        case kIOMessageSystemWillPowerOn:
        case kIOMessageSystemHasPoweredOn:
            if (application)
                emit application->sig_powerEvent(kIOMessageSystemHasPoweredOn);
            break;

        default:
            break;
    }
}
#endif // defined(Q_OS_MACOS)

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

#if defined(Q_OS_MACOS)
    event_monitor_ = std::make_unique<EventMonitor>();
#endif // defined(Q_OS_MACOS)
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

#if defined(Q_OS_MACOS)
    event_monitor_.reset();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
// static
CoreApplication* CoreApplication::instance()
{
    return static_cast<CoreApplication*>(QCoreApplication::instance());
}
