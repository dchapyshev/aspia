//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/ui/host_application.h"

#include "build/version.h"
#include "qt_base/qt_logging.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>

namespace host {

namespace {

const char kActivateMessage[] = "activate";

class EventFilter : public QAbstractNativeEventFilter
{
public:
    ~EventFilter() = default;

    static EventFilter* instance();

    // QAbstractNativeEventFilter implementation.
    bool nativeEventFilter(const QByteArray& event_type, void* message, long* result) override;

private:
    EventFilter() = default;

    DISALLOW_COPY_AND_ASSIGN(EventFilter);
};

// static
EventFilter* EventFilter::instance()
{
    static EventFilter event_filter;
    return &event_filter;
}

bool EventFilter::nativeEventFilter(const QByteArray& event_type, void* message, long* result)
{
    MSG* native_message = reinterpret_cast<MSG*>(message);

    if (native_message->message == WM_QUERYENDSESSION || native_message->message == WM_ENDSESSION)
    {
        *result = TRUE;
        return true;
    }

    return false;
}

} // namespace

Application::Application(int& argc, char* argv[])
    : qt_base::Application(argc, argv)
{
    setOrganizationName(QLatin1String("Aspia"));
    setApplicationName(QLatin1String("Host"));
    setApplicationVersion(QLatin1String(ASPIA_VERSION_STRING));
    setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    QAbstractEventDispatcher::instance()->installNativeEventFilter(
        EventFilter::instance());

    connect(this, &Application::messageReceived, [this](const QByteArray& message)
    {
        if (message == kActivateMessage)
        {
            emit activated();
        }
        else
        {
            LOG(LS_ERROR) << "Unhandled message";
        }
    });

    if (!hasLocale(settings_.locale()))
        settings_.setLocale(QLatin1String(DEFAULT_LOCALE));

    setLocale(settings_.locale());
}

// static
Application* Application::instance()
{
    return static_cast<Application*>(QApplication::instance());
}

void Application::activate()
{
    sendMessage(kActivateMessage);
}

} // namespace host
