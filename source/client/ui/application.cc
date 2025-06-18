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

#include "client/ui/application.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "client/ui/client_settings.h"

#include <QIcon>

namespace client {

//--------------------------------------------------------------------------------------------------
Application::Application(int& argc, char* argv[])
    : base::GuiApplication(argc, argv)
{
    LOG(INFO) << "Ctor";

    setOrganizationName("Aspia");
    setApplicationName("Client");
    setApplicationVersion(ASPIA_VERSION_STRING);
    setWindowIcon(QIcon(":/img/main.ico"));

    ClientSettings settings;

    if (!hasLocale(settings.locale()))
    {
        LOG(INFO) << "Set default locale";
        settings.setLocale(DEFAULT_LOCALE);
    }

    setLocale(settings.locale());
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

} // namespace client
