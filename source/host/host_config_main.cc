//
// PROJECT:         Aspia
// FILE:            host/host_config_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_config_main.h"

#include "base/file_logger.h"
#include "host/ui/user_list_dialog.h"

namespace aspia {

int HostConfigMain(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Host");
    application.setApplicationVersion("1.0.0");

    FileLogger logger;
    logger.startLogging(application);

    UserListDialog dialog;
    dialog.show();
    dialog.activateWindow();

    return application.exec();
}

} // namespace aspia
