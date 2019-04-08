//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/host_session_main.h"

#include "base/base_paths.h"
#include "base/qt_logging.h"
#include "build/version.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/host_session.h"
#include "host/win/host_starter_service.h"

#include <QGuiApplication>
#include <QCommandLineParser>

#include <windows.h>

namespace host {

namespace {

std::filesystem::path loggingDir()
{
    std::filesystem::path path;

    if (!base::BasePaths::commonAppData(&path))
        return std::filesystem::path();

    path.append("aspia/logs");
    return path;
}

int runApplication(int argc, char *argv[])
{
    QStringList arguments;

    // We need to get command line arguments before creating QApplication.
    // Prepare a list of arguments for the parser.
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    QCommandLineOption service_id_option(
        QStringLiteral("service_id"), QString(), QStringLiteral("service_id"));

    QCommandLineOption channel_id_option(
        QStringLiteral("channel_id"), QString(), QStringLiteral("channel_id"));

    QCommandLineOption session_type_option(
        QStringLiteral("session_type"), QString(), QStringLiteral("session_type"));

    QCommandLineParser parser;
    parser.addOption(service_id_option);
    parser.addOption(channel_id_option);
    parser.addOption(session_type_option);

    if (!parser.parse(arguments))
    {
        LOG(LS_WARNING) << "Error parsing command line parameters: " << parser.errorText();
        return 1;
    }

    if (parser.isSet(service_id_option))
    {
        if (parser.isSet(session_type_option) && parser.isSet(channel_id_option))
        {
            StarterService service(parser.value(service_id_option));
            return service.exec(argc, argv);
        }
    }
    else if (parser.isSet(session_type_option) && parser.isSet(channel_id_option))
    {
        // At the end of the user's session, the program ends later than the others.
        SetProcessShutdownParameters(0, SHUTDOWN_NORETRY);

        QGuiApplication application(argc, argv);

        application.setOrganizationName(QStringLiteral("Aspia"));
        application.setApplicationName(QStringLiteral("Host Session"));
        application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

        QScopedPointer<Session> session(
            Session::create(parser.value(session_type_option), parser.value(channel_id_option)));
        if (session)
        {
            session->start();
            return QGuiApplication::exec();
        }
    }

    return 1;
}

} // namespace

int hostSessionMain(int argc, char *argv[])
{
    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = loggingDir();

    base::initLogging(settings);
    base::initQtLogging();

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    int result = runApplication(argc, argv);

    base::shutdownLogging();
    return result;
}

} // namespace host
