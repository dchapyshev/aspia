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

#include "host/ui/host_main.h"

#include <QCommandLineParser>
#include <QSysInfo>
#include <QThread>

#include "base/logging.h"
#include "build/version.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/update_dialog.h"
#include "host/database.h"
#include "host/host_utils.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/host_window.h"
#include "host/ui/security_log_dialog.h"
#include "host/ui/settings_util.h"

#if defined(Q_OS_WINDOWS)
#include "base/process_util.h"
#include "base/win/desktop.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <unistd.h>

#include "base/linux/libx11.h"
#endif // defined(Q_OS_LINUX)

namespace {

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
        Display* display = LibX11::openDisplay(nullptr);
        if (display)
        {
            LibX11::closeDisplay(display);
            break;
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

} // namespace

//--------------------------------------------------------------------------------------------------
int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;

    ScopedLogging scoped_logging(logging_settings);

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

    Application application(argc, argv);
    Application::setQuitOnLastWindowClosed(false);

#if defined(Q_OS_LINUX)
    // Qt's xcb plugin installs its own X11 I/O-error handler in the line above that aborts the process
    // when the Xwayland connection drops (every session end / login switch). Override it so the GUI
    // exits cleanly instead of crashing.
    LibX11::setIoErrorHandler(onX11IoError);
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

    if (!HostUtils::integrityCheck())
    {
        LOG(ERROR) << "Integrity check failed";

        MsgBox::warning(nullptr,
            QApplication::translate("Host", "Application integrity check failed. Components are "
                                            "missing or damaged."));
        return 1;
    }

    LOG(INFO) << "Integrity check passed successfully";

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
        if (application.isRunning())
        {
            LOG(INFO) << "Application already running";
            application.activate(parser.isSet(hidden_option));
        }
        else
        {
            LOG(INFO) << "Application not running yet";

            HostWindow window;
            QObject::connect(&application, &Application::sig_activated,
                             &window, &HostWindow::activateHost);

            if (parser.isSet(hidden_option))
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
