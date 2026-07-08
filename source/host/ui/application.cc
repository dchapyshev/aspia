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

#include "host/ui/application.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QIcon>
#include <QSessionManager>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "host/user_settings.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace {

const char kActivateVisibleMessage[] = "activate_visible";
const char kActivateHiddenMessage[] = "activate_hidden";

class EventFilter final : public QAbstractNativeEventFilter
{
public:
    ~EventFilter() final = default;

    static EventFilter* instance();

    // QAbstractNativeEventFilter implementation.
    bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) final;

private:
    EventFilter() = default;
    Q_DISABLE_COPY_MOVE(EventFilter)
};

//--------------------------------------------------------------------------------------------------
// static
EventFilter* EventFilter::instance()
{
    static EventFilter event_filter;
    return &event_filter;
}

//--------------------------------------------------------------------------------------------------
bool EventFilter::nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result)
{
#if defined(Q_OS_WINDOWS)
    MSG* native_message = reinterpret_cast<MSG*>(message);

    if (native_message->message == WM_QUERYENDSESSION || native_message->message == WM_ENDSESSION)
    {
        *result = TRUE;
        return true;
    }
#endif // defined(Q_OS_WINDOWS)
    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Host");
    setApplicationVersion(ASPIA_VERSION_STRING);
    setWindowIcon(QIcon(":/img/aspia.ico"));

    QAbstractEventDispatcher::instance()->installNativeEventFilter(EventFilter::instance());

#if defined(Q_OS_LINUX)
    // The GUI is owned by the host service, not the desktop session: the service starts it hidden in the
    // tray and restarts it as needed. Tell the session manager never to restore it, so neither ksmserver
    // nor the KDE fallback session-restore relaunches it at the next login - a relaunch arrives without
    // --hidden and pops the main window up over the tray-only instance already started by the service.
    connect(this, &QGuiApplication::saveStateRequest, this, [](QSessionManager& manager)
    {
        manager.setRestartHint(QSessionManager::RestartNever);
    });
#endif // defined(Q_OS_LINUX)

    connect(this, &Application::sig_messageReceived, this, [this](const QByteArray& message)
    {
        if (message == kActivateVisibleMessage)
        {
            LOG(INFO) << "Activate message received";
            emit sig_activated(false);
        }
        else if (message == kActivateHiddenMessage)
        {
            LOG(INFO) << "Activate (hidden) message received";
            emit sig_activated(true);
        }
        else
        {
            LOG(ERROR) << "Unhandled message";
        }
    });

    UserSettings user_settings;
    if (!hasLocale(user_settings.locale()))
    {
        LOG(INFO) << "Set default locale";
        user_settings.setLocale(DEFAULT_LOCALE);
    }

    setTheme(user_settings.theme());
    setLocale(user_settings.locale());
}

//--------------------------------------------------------------------------------------------------
Application::~Application()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
Application* Application::instance()
{
    return static_cast<Application*>(QApplication::instance());
}

//--------------------------------------------------------------------------------------------------
void Application::activate(bool hidden)
{
    LOG(INFO) << "Sending activate message (hidden:" << hidden << ")";
    sendMessage(hidden ? kActivateHiddenMessage : kActivateVisibleMessage);
}
