//
// PROJECT:         Aspia
// FILE:            host/win/host_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/win/host_main.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>

#include "base/file_logger.h"
#include "host/host_session.h"
#include "version.h"

namespace aspia {

int hostMain(int argc, char *argv[])
{
    // At the end of the user's session, the program ends later than the others.
    SetProcessShutdownParameters(0, SHUTDOWN_NORETRY);

    QGuiApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    FileLogger logger;
    logger.startLogging(application);

    QCommandLineOption channel_id_option(QStringLiteral("channel_id"),
                                         QString(),
                                         QStringLiteral("channel_id"));

    QCommandLineOption session_type_option(QStringLiteral("session_type"),
                                           QString(),
                                           QStringLiteral("session_type"));

    QCommandLineParser parser;
    parser.addOption(channel_id_option);
    parser.addOption(session_type_option);

    if (!parser.parse(application.arguments()))
    {
        qWarning() << "Error parsing command line parameters: " << parser.errorText();
        return 1;
    }

    QString channel_id = parser.value(channel_id_option);
    QString session_type = parser.value(session_type_option);

    QPointer<HostSession> session = HostSession::create(session_type, channel_id);
    if (session.isNull())
        return 1;

    session->start();

    return application.exec();
}

} // namespace aspia
