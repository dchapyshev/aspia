//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"
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
    ~EventFilter() override = default;

    static EventFilter* instance();

    // QAbstractNativeEventFilter implementation.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray& event_type, void* message, long* result) override;
#else
    bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) override;
#endif

private:
    EventFilter() = default;

    DISALLOW_COPY_AND_ASSIGN(EventFilter);
};

//--------------------------------------------------------------------------------------------------
// static
EventFilter* EventFilter::instance()
{
    static EventFilter event_filter;
    return &event_filter;
}

//--------------------------------------------------------------------------------------------------
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
bool EventFilter::nativeEventFilter(const QByteArray& event_type, void* message, long* result)
#else
bool EventFilter::nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result)
#endif
{
#if defined(OS_WIN)
    MSG* native_message = reinterpret_cast<MSG*>(message);

    if (native_message->message == WM_QUERYENDSESSION || native_message->message == WM_ENDSESSION)
    {
        *result = TRUE;
        return true;
    }
#endif // defined(OS_WIN)
    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : qt_base::Application(argc, argv)
{
    LOG(LS_INFO) << "Ctor";

    setOrganizationName(QStringLiteral("Aspia"));
    setApplicationName(QStringLiteral("Host"));
    setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
#endif

    QAbstractEventDispatcher::instance()->installNativeEventFilter(
        EventFilter::instance());

    connect(this, &Application::messageReceived, this, [this](const QByteArray& message)
    {
        if (message == kActivateMessage)
        {
            LOG(LS_INFO) << "Activate message received";
            emit activated();
        }
        else
        {
            LOG(LS_ERROR) << "Unhandled message";
        }
    });

    if (!hasLocale(settings_.locale()))
        settings_.setLocale(QStringLiteral(DEFAULT_LOCALE));

    setLocale(settings_.locale());
}

//--------------------------------------------------------------------------------------------------
Application::~Application()
{
    LOG(LS_INFO) << "Dtor";
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
    LOG(LS_INFO) << "Sending activate message";
    sendMessage(kActivateMessage);
}

} // namespace host
