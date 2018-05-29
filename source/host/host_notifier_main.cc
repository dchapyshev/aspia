//
// PROJECT:         Aspia
// FILE:            host/host_notifier_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_notifier_main.h"

#include <QCommandLineParser>
#include <QDebug>

#include "base/file_logger.h"
#include "host/host_notifier.h"
#include "version.h"

namespace aspia {

int HostNotifierMain(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Host");
    application.setApplicationVersion(ASPIA_VERSION_STRING);

    FileLogger logger;
    logger.startLogging(application);

    QCommandLineOption channel_id_option(QStringLiteral("channel_id"),
                                         QStringLiteral("IPC channel id"),
                                         QStringLiteral("channel_id"));
    QCommandLineParser parser;
    parser.addOption(channel_id_option);

    if (!parser.parse(application.arguments()))
    {
        qWarning() << "Error parsing command line parameters: " << parser.errorText();
        return 1;
    }

    HostNotifier notifier;
    notifier.setChannelId(parser.value(channel_id_option));

    if (!notifier.start())
        return 1;

    return application.exec();
}

} // namespace aspia
