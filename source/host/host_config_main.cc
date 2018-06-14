//
// PROJECT:         Aspia
// FILE:            host/host_config_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
