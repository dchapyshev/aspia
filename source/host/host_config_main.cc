//
// PROJECT:         Aspia
// FILE:            host/host_config_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_config_main.h"

#include "base/logging.h"
#include "host/ui/user_list_dialog.h"

namespace aspia {

int HostConfigMain(int argc, char *argv[])
{
    LoggingSettings settings;
    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Host");
    application.setApplicationVersion("1.0.0");

    UserListDialog dialog;
    dialog.show();
    dialog.activateWindow();

    int ret = application.exec();

    ShutdownLogging();
    return ret;
}

} // namespace aspia
