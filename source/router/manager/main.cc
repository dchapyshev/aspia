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

#include "build/version.h"
#include "qt_base/application.h"
#include "router/manager/router_dialog.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(qt_translations);

    qt_base::Application application(argc, argv);

    qt_base::Application::setOrganizationName(QLatin1String("Aspia"));
    qt_base::Application::setApplicationName(QLatin1String("Router Manager"));
    qt_base::Application::setApplicationVersion(QLatin1String(ASPIA_VERSION_STRING));
    qt_base::Application::setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
    qt_base::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    qt_base::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    qt_base::Application::setWindowIcon(QIcon(":/img/main.ico"));

    router::RouterDialog dialog;
    dialog.show();
    dialog.activateWindow();

    return qt_base::Application::exec();
}
