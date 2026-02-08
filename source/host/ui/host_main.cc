//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QMessageBox>
#include <QSysInfo>

#include "base/logging.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/print_debug_info.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/main_window.h"
#include "host/ui/settings_util.h"
#include "common/ui/update_dialog.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/process_util.h"
#include "base/win/desktop.h"
#endif // defined(Q_OS_WINDOWS)

namespace {

//--------------------------------------------------------------------------------------------------
bool waitForValidInputDesktop()
{
#if defined(Q_OS_WINDOWS)
    int max_attempt_count = 600;

    do
    {
        base::Desktop input_desktop(base::Desktop::inputDesktop());
        if (input_desktop.isValid())
        {
            if (input_desktop.setThreadDesktop())
            {
                wchar_t desktop_name[100] = { 0 };
                if (input_desktop.name(desktop_name, sizeof(desktop_name)))
                {
                    LOG(INFO) << "Attached to desktop:" << desktop_name;
                }
                break;
            }
        }

        Sleep(100);
    }
    while (--max_attempt_count > 0);

    if (max_attempt_count == 0)
    {
        LOG(ERROR) << "Exceeded the number of attempts";
        return false;
    }
#endif // defined(Q_OS_WINDOWS)

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

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

    host::Application::setApplicationVersion(ASPIA_VERSION_STRING);
    host::Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    host::Application application(argc, argv);
    host::Application::setQuitOnLastWindowClosed(false);

    host::printDebugInfo();

    QCommandLineOption hidden_option("hidden",
        base::GuiApplication::translate("HostMain", "Launch the application hidden."));
    QCommandLineOption export_option("export",
        base::GuiApplication::translate("HostMain", "Export parameters to file."), "export");
    QCommandLineOption import_option("import",
        base::GuiApplication::translate("HostMain", "Import parameters from file."), "import");
    QCommandLineOption silent_option("silent",
        base::GuiApplication::translate("HostMain", "Do not display any messages during import and export."));
    QCommandLineOption update_option("update",
        base::GuiApplication::translate("HostMain", "Calling the update check dialog."));
    QCommandLineOption config_option("config",
        base::GuiApplication::translate("HostMain", "Calling the settings dialog."));

    QCommandLineParser parser;
    parser.addOption(hidden_option);
    parser.addOption(export_option);
    parser.addOption(import_option);
    parser.addOption(silent_option);
    parser.addOption(update_option);
    parser.addOption(config_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    if (!host::integrityCheck())
    {
        LOG(ERROR) << "Integrity check failed";

        QMessageBox::warning(
            nullptr,
            QApplication::translate("Host", "Warning"),
            QApplication::translate("Host", "Application integrity check failed. Components are "
                                            "missing or damaged."),
            QMessageBox::Ok);
        return 1;
    }

    LOG(INFO) << "Integrity check passed successfully";

    if (parser.isSet(import_option) && parser.isSet(export_option))
    {
        LOG(ERROR) << "Import and export are specified at the same time";

        if (!parser.isSet(silent_option))
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Host", "Warning"),
                QApplication::translate("Host", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (parser.isSet(import_option))
    {
        if (!host::SettingsUtil::importFromFile(parser.value(import_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(export_option))
    {
        if (!host::SettingsUtil::exportToFile(parser.value(export_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(update_option))
    {
        common::UpdateDialog dialog(host::SystemSettings().updateServer(), "host");
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else if (parser.isSet(config_option))
    {
#if defined(Q_OS_WINDOWS)
        if (!base::isProcessElevated())
        {
            LOG(INFO) << "Process not eleavated";
        }
        else
#endif // defined(Q_OS_WINDOWS)
        {
            host::SystemSettings settings;
            if (settings.passwordProtection())
            {
                host::CheckPasswordDialog dialog;
                if (dialog.exec() == host::CheckPasswordDialog::Accepted)
                {
                    host::ConfigDialog().exec();
                }
            }
            else
            {
                host::ConfigDialog().exec();
            }
        }
    }
    else
    {
        if (application.isRunning())
        {
            LOG(INFO) << "Application already running";
            application.activate();
        }
        else
        {
            LOG(INFO) << "Application not running yet";

            host::MainWindow window;
            QObject::connect(&application, &host::Application::sig_activated,
                             &window, &host::MainWindow::activateHost);

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
