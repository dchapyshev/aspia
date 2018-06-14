//
// PROJECT:         Aspia
// FILE:            host/host_notifier_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_notifier_main.h"

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // defined(Q_OS_WIN)

#include <QCommandLineParser>
#include <QFileInfo>
#include <QScreen>

#include "base/file_logger.h"
#include "desktop_capture/win/scoped_thread_desktop.h"
#include "host/ui/host_notifier_window.h"
#include "version.h"

namespace aspia {

int hostNotifierMain(int argc, char *argv[])
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
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    QCommandLineOption channel_id_option(QStringLiteral("channel_id"),
                                         QString(),
                                         QStringLiteral("channel_id"));
    QCommandLineParser parser;
    parser.addOption(channel_id_option);

    if (!parser.parse(application.arguments()))
    {
        qWarning() << "Error parsing command line parameters: " << parser.errorText();
        return 1;
    }

    HostNotifierWindow window;
    window.setChannelId(parser.value(channel_id_option));
    window.show();

    QSize screen_size = QApplication::primaryScreen()->availableSize();
    QSize window_size = window.frameSize();

    window.move(screen_size.width() - window_size.width(),
                screen_size.height() - window_size.height());

#if defined(Q_OS_WIN)
    DWORD active_thread_id = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetForegroundWindow(reinterpret_cast<HWND>(window.winId()));
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
    }

    // I not found a way better than using WinAPI function in MS Windows.
    // Flag Qt::WindowStaysOnTopHint works incorrectly.
    SetWindowPos(reinterpret_cast<HWND>(window.winId()),
                 HWND_TOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else // defined(Q_OS_WIN)
    window.setWindowFlags(
        window.windowFlags() | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
    window.show();
#endif // defined(Q_OS_WIN)

    return application.exec();
}

} // namespace aspia
