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

#include "host/ui/application.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QIcon>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "host/ui/user_settings.h"

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif // defined(Q_OS_WIN)

namespace host {

namespace {

const char kActivateMessage[] = "activate";

class EventFilter final : public QAbstractNativeEventFilter
{
public:
    ~EventFilter() final = default;

    static EventFilter* instance();

    // QAbstractNativeEventFilter implementation.
    bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) final;

private:
    EventFilter() = default;
    Q_DISABLE_COPY(EventFilter)
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
    : base::GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Host");
    setApplicationVersion(ASPIA_VERSION_STRING);
    setWindowIcon(QIcon(":/img/main.ico"));

    QAbstractEventDispatcher::instance()->installNativeEventFilter(
        EventFilter::instance());

    connect(this, &Application::sig_messageReceived, this, [this](const QByteArray& message)
    {
        if (message == kActivateMessage)
        {
            LOG(INFO) << "Activate message received";
            emit sig_activated();
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
void Application::activate()
{
    LOG(INFO) << "Sending activate message";
    sendMessage(kActivateMessage);
}

} // namespace host
