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

#include "client/application.h"

#include "version.h"
#include "base/build_config.h"
#include "base/logging.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/workers/router_worker.h"
#include "client/workers/update_worker.h"

#if !defined(Q_OS_ANDROID)
#include "client/mcp/mcp_worker.h"
#endif // !defined(Q_OS_ANDROID)

#include <QIcon>

namespace {

const char kActivateWindow[] = "activate_window";

} // namespace

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Client");
    setApplicationVersion(ASPIA_VERSION_STRING);
#if !defined(Q_OS_MACOS)
    setWindowIcon(QIcon(":/img/aspia.ico"));
#endif // !defined(Q_OS_MACOS)

    connect(this, &Application::sig_messageReceived, this, [this](const QByteArray& message)
    {
        if (message.startsWith(kActivateWindow))
        {
            emit sig_windowActivated();
        }
        else
        {
            LOG(ERROR) << "Unhandled message";
        }
    });

    Settings settings;

    QString locale = resolveLocale(settings.locale());
    if (locale != settings.locale())
    {
        LOG(INFO) << "Locale changed to:" << locale;
        settings.setLocale(locale);
    }

    setLocale(locale);
    setTheme(settings.theme());

    addWorker(std::make_unique<RouterWorker>());
    addWorker(std::make_unique<UpdateWorker>());

#if !defined(Q_OS_ANDROID)
    if (Database::instance().isMcpEnabled())
        addWorker(std::make_unique<McpWorker>());
#endif // !defined(Q_OS_ANDROID)
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
void Application::activateWindow()
{
    sendMessage(kActivateWindow);
}
