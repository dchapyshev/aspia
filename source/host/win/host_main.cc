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

#include "host/win/host_main.h"

#include <qt_windows.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>
#include <QScreen>

#include "base/file_logger.h"
#include "desktop_capture/win/scoped_thread_desktop.h"
#include "host/ui/host_notifier_window.h"
#include "host/host_session.h"
#include "version.h"

namespace aspia {

namespace {

int runHostSession(const QString& channel_id, const QString& session_type)
{
    // At the end of the user's session, the program ends later than the others.
    SetProcessShutdownParameters(0, SHUTDOWN_NORETRY);

    QScopedPointer<HostSession> session(HostSession::create(session_type, channel_id));
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

    QSize screen_size = QApplication::primaryScreen()->availableSize();
    QSize window_size = window.frameSize();

    window.move(screen_size.width() - window_size.width(),
                screen_size.height() - window_size.height());

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

} // namespace

int hostMain(int argc, char *argv[])
{
    FileLogger logger;
    logger.startLogging(QFileInfo(argv[0]).fileName());

    int max_attempt_count = 600;

    do
    {
        Desktop input_desktop(Desktop::inputDesktop());
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
        qWarning("Exceeded the number of attempts");
        return 1;
    }

    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

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
    if (channel_id.isEmpty())
    {
        qWarning("Empty channel id");
        return 1;
    }

    if (parser.isSet(session_type_option))
        return runHostSession(channel_id, parser.value(session_type_option));

    return runHostNotifier(channel_id);
}

} // namespace aspia
