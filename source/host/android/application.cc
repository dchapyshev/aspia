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

#include "host/android/application.h"

#include <QIcon>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "host/user_settings.h"
#include "host/android/server_worker.h"

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Host");
    setApplicationVersion(ASPIA_VERSION_STRING);
    setWindowIcon(QIcon(":/img/aspia.ico"));

    UserSettings settings;

    if (!hasLocale(settings.locale()))
        settings.setLocale(DEFAULT_LOCALE);

    setLocale(settings.locale());
    setTheme(settings.theme());

    addWorker(std::make_unique<ServerWorker>());
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
