//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_config_main.h"

#include <QFileInfo>

#include "base/file_logger.h"
#include "host/ui/host_config_dialog.h"
#include "version.h"

namespace aspia {

int hostConfigMain(int argc, char *argv[])
{
    FileLogger logger;
    logger.startLogging(QFileInfo(argv[0]).fileName());

    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    HostConfigDialog dialog;
    dialog.show();
    dialog.activateWindow();

    return application.exec();
}

} // namespace aspia
