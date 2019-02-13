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

#include <QApplication>
#include <QCommandLineParser>

#include "base/win/scoped_thread_desktop.h"
#include "base/base_paths.h"
#include "base/qt_logging.h"
#include "build/version.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/ui/host_notifier_window.h"
#include "host/host_session.h"

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

bool waitForValidInputDesktop()
{
    int max_attempt_count = 600;

    do
    {
        base::Desktop input_desktop(base::Desktop::inputDesktop());
        if (input_desktop.isValid())
        {
            if (input_desktop.setThreadDesktop())
                break;
        }

        Sleep(100);
    }
    while (--max_attempt_count > 0);

    if (max_attempt_count == 0)
    {
        LOG(LS_WARNING) << "Exceeded the number of attempts";
        return false;
    }

    return true;
}

int runHostSession(const QString& channel_id, const QString& session_type)
{
    // At the end of the user's session, the program ends later than the others.
    SetProcessShutdownParameters(0, SHUTDOWN_NORETRY);

    QScopedPointer<Session> session(Session::create(session_type, channel_id));
    if (session.isNull())
        return 1;

    session->start();

    return QApplication::exec();
}

int runHostNotifier(const QString& channel_id)
{
    HostNotifierWindow window;
    window.setChannelId(channel_id);
    window.show();

    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(reinterpret_cast<HWND>(window.winId()));
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    return QApplication::exec();
}

int runApplication(int argc, char *argv[])
{
    QStringList arguments;

    // We need to get command line arguments before creating QApplication.
    // Prepare a list of arguments for the parser.
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));

    QCommandLineOption channel_id_option(
        QStringLiteral("channel_id"), QString(), QStringLiteral("channel_id"));

    QCommandLineOption session_type_option(
        QStringLiteral("session_type"), QString(), QStringLiteral("session_type"));

    QCommandLineParser parser;
    parser.addOption(channel_id_option);
    parser.addOption(session_type_option);

    if (!parser.parse(arguments))
    {
        LOG(LS_WARNING) << "Error parsing command line parameters: " << parser.errorText();
        return 1;
    }

    QString channel_id = parser.value(channel_id_option);
    if (channel_id.isEmpty())
    {
        LOG(LS_WARNING) << "Empty channel id";
        return 1;
    }

    if (parser.isSet(session_type_option))
    {
        QGuiApplication application(argc, argv);

        application.setOrganizationName(QStringLiteral("Aspia"));
        application.setApplicationName(QStringLiteral("Host Session"));
        application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

        return runHostSession(channel_id, parser.value(session_type_option));
    }
    else
    {
        if (!waitForValidInputDesktop())
            return 1;

        QApplication application(argc, argv);

        application.setOrganizationName(QStringLiteral("Aspia"));
        application.setApplicationName(QStringLiteral("Host Notifier"));
        application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

        return runHostNotifier(channel_id);
    }
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
