//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QCommandLineParser>
#include <QFile>
#include <QIODevice>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

#include "base/asio_event_dispatcher.h"
#include "base/core_application.h"
#include "base/ipc/ipc_server.h"
#include "base/logging.h"
#include "base/service_controller.h"
#include "build/version.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/update_dialog.h"
#include "host/database.h"
#include "host/desktop_agent.h"
#include "host/file_agent.h"
#include "host/host_utils.h"
#include "host/service.h"
#include "host/settings_util.h"
#include "host/system_settings.h"
#include "host/terminal_agent.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/host_window.h"
#include "host/ui/security_log_dialog.h"

#if defined(Q_OS_WINDOWS)
#include <Windows.h>

#include "base/process_util.h"
#include "base/win/desktop.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <unistd.h>

#include "base/linux/x11_headers.h"
#endif // defined(Q_OS_LINUX)

namespace {

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
// The desktop agent captures the screen and maps input coordinates, so it must be per-monitor DPI
// aware to work in physical pixels on scaled displays; the other agents do not care either way. The
// GUI gets this from Qt (QApplication), but a headless agent runs on QCoreApplication, which does not
// set it - and the shared binary uses the GUI manifest, which deliberately leaves DPI awareness unset
// so Qt can select Per-Monitor V2.
//
// The build targets Windows 7, so the newer entry points are not in the headers; resolve the best
// available one at runtime: Per-Monitor-V2 (Win10 1703+), then Per-Monitor (Win8.1+), then system
// aware (always present).
void setDpiAwareness()
{
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
    {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        auto set_context = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));

        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (DPI_AWARENESS_CONTEXT)-4.
        if (set_context && set_context(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4))))
            return;
    }

    if (HMODULE shcore = LoadLibraryW(L"shcore.dll"))
    {
        using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
        auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));

        // PROCESS_PER_MONITOR_DPI_AWARE == 2.
        const bool ok = set_awareness && SUCCEEDED(set_awareness(2));
        FreeLibrary(shcore);
        if (ok)
            return;
    }

    // Windows Vista+ fallback: system DPI aware.
    SetProcessDPIAware();
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
// Runs the host as a headless session agent; |session_type| is the "--session-type" value straight
// from the command line ("desktop", "file" or "terminal"). The agent is the same aspia_host binary as
// the GUI, so it presents the same code identity to the OS - on macOS that is what lets it inherit the
// app's privacy (TCC) grants instead of being a separate app. Returns the process exit code.
int runAgent(int& argc, char* argv[], const char* session_type)
{
    const bool desktop = qstrcmp(session_type, "desktop") == 0;
    const bool file = qstrcmp(session_type, "file") == 0;
    const bool terminal = qstrcmp(session_type, "terminal") == 0;

    if (!desktop && !file && !terminal)
    {
        LOG(ERROR) << "Unknown --session-type value:" << session_type;
        return 1;
    }

#if defined(Q_OS_WINDOWS)
    setDpiAwareness();
#endif // defined(Q_OS_WINDOWS)

    // On macOS the desktop agent captures the screen on a Qt worker thread that needs a real CFRunLoop
    // (the capture/display APIs deliver on the run loop). Make Qt back its stock QThread dispatchers
    // with CoreFoundation so those threads get one; other platforms and agents simply ignore the
    // variable. The main thread keeps AsioEventDispatcher below.
    qputenv("QT_EVENT_DISPATCHER_CORE_FOUNDATION", "1");

    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);
    CoreApplication application(argc, argv);

    if (desktop)
    {
        HostUtils::printDebugInfo(
            HostUtils::INCLUDE_VIDEO_ADAPTERS | HostUtils::INCLUDE_WINDOW_STATIONS);
    }
    else
    {
        HostUtils::printDebugInfo();
    }

    QString channel_id = qEnvironmentVariable(IpcServer::kChannelIdEnvVar);
    if (channel_id.isEmpty())
    {
        LOG(ERROR) << "Environment variable" << IpcServer::kChannelIdEnvVar << "is not set";
        return 1;
    }

    if (desktop)
    {
        DesktopAgent agent;
        agent.start(channel_id);
        return application.exec();
    }

    if (file)
    {
        FileAgent agent;
        agent.start(channel_id);
        return application.exec();
    }

    TerminalAgent agent;
    agent.start(channel_id);
    return application.exec();
}

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->start())
    {
        out << "Failed to start the service." << Qt::endl;
        return 1;
    }

    out << "The service started successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int stopService(QTextStream& out)
{
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->stop())
    {
        out << "Failed to stop the service." << Qt::endl;
        return 1;
    }

    out << "The service has stopped successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int installService(QTextStream& out)
{
    // The service is the shared aspia_host binary; register it to run in service mode so the OS starts
    // "aspia_host --service".
    std::unique_ptr<ServiceController> controller = ServiceController::install(
        Service::kName, Service::kDisplayName, QCoreApplication::applicationFilePath(),
        QStringList{ "--service" });
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(Service::kDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (ServiceController::isRunning(Service::kName))
        stopService(out);

    if (!ServiceController::remove(Service::kName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
// Runs a one-shot service management |command| (--install/--remove/--start/--stop). Such a command
// needs no service machinery - just a core application for applicationFilePath() and process
// spawning.
int runServiceCommand(int& argc, char* argv[], int (*command)(QTextStream& out))
{
    QCoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);
    QCoreApplication application(argc, argv);

    QTextStream out(stdout, QIODevice::WriteOnly);
    return command(out);
}

//--------------------------------------------------------------------------------------------------
// Runs the host as the background service ("--service" - what the OS service registration points at).
// Relies on the caller's ScopedLogging.
int runService(int& argc, char* argv[])
{
    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);

    CoreApplication application(argc, argv);

    HostUtils::printDebugInfo();

    return Service().exec(application);
}

#if defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// Called by Xlib when the X11 (Xwayland) connection is lost. The host GUI runs on Xwayland, so this
// fires whenever the user's session ends or the active session switches at login. Exit cleanly here
// instead of letting Qt's xcb plugin abort the process; the host service starts a fresh GUI for the
// new session.
int onX11IoError(Display* /* display */)
{
    LOG(INFO) << "X11 connection lost (session ended or switched); exiting host GUI";
    _exit(0);
    return 0;
}
#endif // defined(Q_OS_LINUX)

//--------------------------------------------------------------------------------------------------
bool waitForValidInputDesktop()
{
    int max_attempt_count = 600;

#if defined(Q_OS_LINUX)
    // Must stay well under the service's attach timeout: a compositor without XSETTINGS at all (e.g.
    // wlroots-based) never produces an owner, and a GUI still waiting when the timeout hits is killed
    // and relaunched in an endless loop, never reaching the give-up branch below.
    int xsettings_attempt_count = 80;
#endif

    do
    {
#if defined(Q_OS_WINDOWS)
        Desktop input_desktop(Desktop::inputDesktop());
        if (input_desktop.isValid() && input_desktop.setThreadDesktop())
        {
            wchar_t desktop_name[100] = { 0 };
            if (input_desktop.name(desktop_name, sizeof(desktop_name)))
                LOG(INFO) << "Attached to desktop:" << desktop_name;
            break;
        }
#elif defined(Q_OS_LINUX)
        // The service may launch the GUI a moment before the session's X server (Xwayland) is accepting
        // connections; creating the QApplication then fails and the GUI exits. Wait until the display
        // can be opened.
        Display* display = XOpenDisplay(nullptr);
        if (display)
        {
            // Qt's xcb backend takes font DPI/scaling (Xft/DPI) from the XSETTINGS manager and follows
            // its runtime changes, but only when the manager selection already has an owner at
            // application startup - an application started earlier never reconnects to it. The DE's
            // xsettings daemon can come up tens of seconds after the X server right after login, so
            // also wait for the owner (bounded separately; a DE that has none proceeds after the
            // timeout).
            const QByteArray selection_name =
                "_XSETTINGS_S" + QByteArray::number(DefaultScreen(display));
            const Atom selection = XInternAtom(display, selection_name.constData(), 0);
            const bool has_xsettings = XGetSelectionOwner(display, selection) != 0;

            XCloseDisplay(display);

            if (has_xsettings)
                break;

            if (--xsettings_attempt_count == 0)
            {
                LOG(WARNING) << "XSETTINGS manager did not appear; the GUI may not pick up display scaling";
                break;
            }
        }
#else
        break;
#endif // defined(Q_OS_WINDOWS)
        QThread::msleep(100);
    }
    while (--max_attempt_count > 0);

    if (max_attempt_count == 0)
    {
        LOG(ERROR) << "Timed out waiting for a valid input desktop";
        return false;
    }

    return true;
}

#if defined(Q_OS_LINUX)
//--------------------------------------------------------------------------------------------------
// True when this process was launched by a desktop session restore rather than by the host service or
// the user. XSMP session managers (ksmserver, gnome-session) export DESKTOP_AUTOSTART_ID for the clients
// they relaunch; KDE Plasma restores non-XSMP apps through plasma-fallback-session-restore, which runs
// them inside its own systemd user service, so the process cgroup carries its name. Such a launch must
// not pop the main window: the service already runs the real hidden instance, and showing the window
// here also makes the window manager save it and restore it again on the next login (a self-sustaining
// loop).
bool isSessionRestoreLaunch()
{
    if (qEnvironmentVariableIsSet("DESKTOP_AUTOSTART_ID"))
        return true;

    // plasma-fallback-session-restore runs the app under its own systemd user service, whose name
    // contains "session-restore". Depending on how it is spawned that shows up either in the process
    // cgroup or in the MEMORY_PRESSURE_WATCH path systemd exports from the same service. systemd
    // escapes the dashes of the unit name as \x2d in those paths, so normalize before matching.
    QByteArray unit_paths = qgetenv("MEMORY_PRESSURE_WATCH");

    QFile cgroup("/proc/self/cgroup");
    if (cgroup.open(QIODevice::ReadOnly))
        unit_paths += cgroup.readAll();

    unit_paths.replace("\\x2d", "-");

    return unit_paths.contains("session-restore");
}
#endif // defined(Q_OS_LINUX)

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;

    ScopedLogging scoped_logging(logging_settings);

    for (int i = 1; i < argc; ++i)
    {
        if (qstrcmp(argv[i], "--service") == 0)
            return runService(argc, argv);
        if (qstrcmp(argv[i], "--install") == 0)
            return runServiceCommand(argc, argv, installService);
        if (qstrcmp(argv[i], "--remove") == 0)
            return runServiceCommand(argc, argv, removeService);
        if (qstrcmp(argv[i], "--start") == 0)
            return runServiceCommand(argc, argv, startService);
        if (qstrcmp(argv[i], "--stop") == 0)
            return runServiceCommand(argc, argv, stopService);
        if (qstrcmp(argv[i], "--session-type") == 0 && i + 1 < argc)
            return runAgent(argc, argv, argv[i + 1]);
    }

    for (int i = 0; i < argc; ++i)
    {
        if (qstrcmp(argv[i], "--hidden") == 0)
        {
            LOG(INFO) << "Has 'hidden' switch";
            if (!waitForValidInputDesktop())
                return 1;
            break;
        }
    }

#if defined(Q_OS_LINUX)
    // The host GUI (tray, notifier, dialogs) does nothing Wayland-specific, but a native Wayland client
    // cannot position or stack its own windows - the notifier has to sit in a screen corner and stay on
    // top. Run the GUI through Xwayland, where the existing X11 window management works on every
    // compositor (GNOME exposes no positioning protocol natively). Screen capture and input stay native
    // Wayland in the separate agent process via the portal.
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    Application::setApplicationVersion(ASPIA_VERSION_STRING);
    Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

#if defined(Q_OS_MACOS)
    // The settings / security-log dialog is launched elevated through Authorization Services, which runs
    // the process with euid 0 but the original real uid. Qt would otherwise abort on that uid mismatch
    // ("running setuid, this is a security hole"). The launch is our own, so allow it.
    Application::setSetuidAllowed(true);
#endif // defined(Q_OS_MACOS)

    Application application(argc, argv);
    Application::setQuitOnLastWindowClosed(false);

#if defined(Q_OS_LINUX)
    // Qt's xcb plugin installs its own X11 I/O-error handler in the line above that aborts the process
    // when the Xwayland connection drops (every session end / login switch). Override it so the GUI
    // exits cleanly instead of crashing.
    XSetIOErrorHandler(onX11IoError);
#endif

    LOG(INFO) << "QPA platform:" << Application::platformName();

    HostUtils::printDebugInfo();

    QCommandLineOption hidden_option("hidden",
        GuiApplication::translate("HostMain", "Launch the application hidden."));
    QCommandLineOption export_option("export",
        GuiApplication::translate("HostMain", "Export parameters to file."), "export");
    QCommandLineOption import_option("import",
        GuiApplication::translate("HostMain", "Import parameters from file."), "import");
    QCommandLineOption silent_option("silent",
        GuiApplication::translate("HostMain", "Do not display any messages during import and export."));
    QCommandLineOption update_option("update",
        GuiApplication::translate("HostMain", "Calling the update check dialog."));
    QCommandLineOption config_option("config",
        GuiApplication::translate("HostMain", "Calling the settings dialog."));
    QCommandLineOption security_log_option("security-log",
        GuiApplication::translate("HostMain", "Calling the security log dialog."));

    QCommandLineParser parser;
    parser.addOption(hidden_option);
    parser.addOption(export_option);
    parser.addOption(import_option);
    parser.addOption(silent_option);
    parser.addOption(update_option);
    parser.addOption(config_option);
    parser.addOption(security_log_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    if (parser.isSet(import_option) && parser.isSet(export_option))
    {
        LOG(ERROR) << "Import and export are specified at the same time";

        if (!parser.isSet(silent_option))
        {
            MsgBox::warning(nullptr,
                QApplication::translate("Host", "Export and import parameters can not be specified together."));
        }

        return 1;
    }
    else if (parser.isSet(import_option))
    {
        if (!SettingsUtil::importFromFile(parser.value(import_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(export_option))
    {
        if (!SettingsUtil::exportToFile(parser.value(export_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(update_option))
    {
        UpdateDialog dialog(SystemSettings().updateServer(), "host");
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else if (parser.isSet(config_option))
    {
#if defined(Q_OS_WINDOWS)
        if (!ProcessUtil::isProcessElevated())
        {
            LOG(INFO) << "Process not eleavated";
        }
        else
#endif // defined(Q_OS_WINDOWS)
        {
            switch (Database::instance().passwordProtectionState())
            {
                case Database::PasswordProtection::DISABLED:
                {
                    ConfigDialog().exec();
                }
                break;

                case Database::PasswordProtection::ENABLED:
                {
                    CheckPasswordDialog dialog;
                    if (dialog.exec() == CheckPasswordDialog::Accepted)
                        ConfigDialog().exec();
                }
                break;

                case Database::PasswordProtection::UNAVAILABLE:
                {
                    MsgBox::warning(nullptr,
                        QApplication::translate("Host", "Settings storage is unavailable."));
                }
                break;
            }
        }
    }
    else if (parser.isSet(security_log_option))
    {
#if defined(Q_OS_WINDOWS)
        if (!ProcessUtil::isProcessElevated())
        {
            LOG(INFO) << "Process not eleavated";
        }
        else
#endif // defined(Q_OS_WINDOWS)
        {
            switch (Database::instance().passwordProtectionState())
            {
                case Database::PasswordProtection::DISABLED:
                {
                    SecurityLogDialog().exec();
                }
                break;

                case Database::PasswordProtection::ENABLED:
                {
                    CheckPasswordDialog dialog;
                    if (dialog.exec() == CheckPasswordDialog::Accepted)
                        SecurityLogDialog().exec();
                }
                break;

                case Database::PasswordProtection::UNAVAILABLE:
                {
                    MsgBox::warning(nullptr,
                        QApplication::translate("Host", "Settings storage is unavailable."));
                }
                break;
            }
        }
    }
    else
    {
        bool hidden = parser.isSet(hidden_option);

#if defined(Q_OS_LINUX)
        if (!hidden && isSessionRestoreLaunch())
        {
            LOG(INFO) << "Launched by desktop session restore; starting hidden";
            hidden = true;
        }
#endif // defined(Q_OS_LINUX)

        if (application.isRunning())
        {
            LOG(INFO) << "Application already running";
            application.activate(hidden);
        }
        else
        {
            LOG(INFO) << "Application not running yet";

            HostWindow window;
            QObject::connect(&application, &Application::sig_activated,
                             &window, &HostWindow::activateHost);

            if (hidden)
            {
                LOG(INFO) << "Hide window to tray";
                window.hideToTray();
            }
            else
            {
                LOG(INFO) << "Show window";
                window.show();
                window.activateWindow();
            }

            window.connectToService();

            return application.exec();
        }
    }

    return 0;
}
