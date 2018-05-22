//
// PROJECT:         Aspia
// FILE:            host/host_config_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_config_main.h"

#include "base/file_logger.h"
#include "host/ui/host_config_dialog.h"
#include "version.h"

namespace aspia {

int HostConfigMain(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Host");
    application.setApplicationVersion(ASPIA_VERSION_STRING);

    FileLogger logger;
    logger.startLogging(application);

    HostConfigDialog dialog;
    dialog.show();
    dialog.activateWindow();

    return application.exec();
}

} // namespace aspia
