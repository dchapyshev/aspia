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
#include <QDebug>
#include <QScreen>

#include "base/file_logger.h"
#include "host/ui/host_notifier_window.h"
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

    HostNotifierWindow window;
    window.setChannelId(parser.value(channel_id_option));
    window.show();

    QSize screen_size = QApplication::primaryScreen()->availableSize();
    QSize window_size = window.frameSize();

    window.move(screen_size.width() - window_size.width(),
                screen_size.height() - window_size.height());

#if defined(Q_OS_WIN)
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
