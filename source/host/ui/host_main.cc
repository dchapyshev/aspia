//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/command_line.h"
#include "base/environment.h"
#include "base/sys_info.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/system_settings.h"
#include "host/ui/application.h"
#include "host/ui/check_password_dialog.h"
#include "host/ui/config_dialog.h"
#include "host/ui/main_window.h"
#include "host/ui/settings_util.h"
#include "common/ui/update_dialog.h"
#include "qt_base/scoped_qt_logging.h"

#if defined(OS_WIN)
#include "base/win/mini_dump_writer.h"
#include "base/win/process_util.h"
#include "base/win/scoped_thread_desktop.h"
#include "base/win/session_info.h"
#endif // defined(OS_WIN)

#include <QMessageBox>

namespace {

bool waitForValidInputDesktop()
{
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
                    LOG(LS_INFO) << "Attached to desktop: " << desktop_name;
                }
                break;
            }
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

} // namespace

int hostMain(int argc, char* argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Q_INIT_RESOURCE(qt_translations);
#else
    Q_INIT_RESOURCE(qt_translations6);
#endif
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

#if defined(OS_WIN)
    base::installFailureHandler(L"aspia_host");
#endif

    qt_base::ScopedQtLogging scoped_logging;
    base::CommandLine command_line(argc, argv);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "Command line: " << command_line.commandLineString();
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

#if defined(OS_WIN)
    MEMORYSTATUSEX memory_status;
    memset(&memory_status, 0, sizeof(memory_status));
    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status))
    {
        static const uint32_t kMB = 1024 * 1024;

        LOG(LS_INFO) << "Total physical memory: " << (memory_status.ullTotalPhys / kMB)
                     << "MB (free: " << (memory_status.ullAvailPhys / kMB) << "MB)";
        LOG(LS_INFO) << "Total page file: " << (memory_status.ullTotalPageFile / kMB)
                     << "MB (free: " << (memory_status.ullAvailPageFile / kMB) << "MB)";
        LOG(LS_INFO) << "Total virtual memory: " << (memory_status.ullTotalVirtual / kMB)
                     << "MB (free: " << (memory_status.ullAvailVirtual / kMB) << "MB)";
    }
    else
    {
        PLOG(LS_WARNING) << "GlobalMemoryStatusEx failed";
    }

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_WARNING) << "ProcessIdToSessionId failed";
    }
    else
    {
        base::win::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_WARNING) << "Unable to get session info";
        }
        else
        {
            LOG(LS_INFO) << "Process session ID: " << session_id;
            LOG(LS_INFO) << "Running in user session: '" << session_info.userName() << "'";
            LOG(LS_INFO) << "Session connect state: "
                << base::win::SessionInfo::connectStateToString(session_info.connectState());
            LOG(LS_INFO) << "WinStation name: '" << session_info.winStationName() << "'";
            LOG(LS_INFO) << "Domain name: '" << session_info.domain() << "'";
        }
    }

    wchar_t username[64] = { 0 };
    DWORD username_size = sizeof(username) / sizeof(username[0]);
    if (!GetUserNameW(username, &username_size))
    {
        PLOG(LS_WARNING) << "GetUserNameW failed";
    }

    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admins_group = nullptr;
    BOOL is_user_admin = AllocateAndInitializeSid(&nt_authority,
                                                  2,
                                                  SECURITY_BUILTIN_DOMAIN_RID,
                                                  DOMAIN_ALIAS_RID_ADMINS,
                                                  0, 0, 0, 0, 0, 0,
                                                  &admins_group);
    if (is_user_admin)
    {
        if (!CheckTokenMembership(nullptr, admins_group, &is_user_admin))
        {
            PLOG(LS_WARNING) << "CheckTokenMembership failed";
            is_user_admin = FALSE;
        }
        FreeSid(admins_group);
    }
    else
    {
        PLOG(LS_WARNING) << "AllocateAndInitializeSid failed";
    }

    LOG(LS_INFO) << "Running as user: '" << username << "'";
    LOG(LS_INFO) << "Member of admins group: " << (is_user_admin ? "Yes" : "No");
    LOG(LS_INFO) << "Process elevated: " << (base::win::isProcessElevated() ? "Yes" : "No");
    LOG(LS_INFO) << "Active console session ID: " << WTSGetActiveConsoleSessionId();
    LOG(LS_INFO) << "Computer name: '" << base::SysInfo::computerName() << "'";

    LOG(LS_INFO) << "Environment variables";
    LOG(LS_INFO) << "#####################################################";
    for (const auto& variable : base::Environment::list())
    {
        LOG(LS_INFO) << variable.first << ": " << variable.second;
    }
    LOG(LS_INFO) << "#####################################################";
#endif

    bool is_hidden = command_line.hasSwitch(u"hidden");
    if (is_hidden)
    {
        LOG(LS_INFO) << "Has 'hidden' switch";

        if (!waitForValidInputDesktop())
            return 1;
    }

    host::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    host::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    host::Application application(argc, argv);

    if (!host::integrityCheck())
    {
        LOG(LS_WARNING) << "Integrity check failed";

        QMessageBox::warning(
            nullptr,
            QApplication::translate("Host", "Warning"),
            QApplication::translate("Host", "Application integrity check failed. Components are "
                                            "missing or damaged."),
            QMessageBox::Ok);
        return 1;
    }
    else
    {
        LOG(LS_INFO) << "Integrity check passed successfully";
    }

    if (command_line.hasSwitch(u"import") && command_line.hasSwitch(u"export"))
    {
        LOG(LS_WARNING) << "Import and export are specified at the same time";

        if (!command_line.hasSwitch(u"silent"))
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Host", "Warning"),
                QApplication::translate("Host", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (command_line.hasSwitch(u"import"))
    {
        if (!host::SettingsUtil::importFromFile(command_line.switchValuePath(u"import"),
                                                command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"export"))
    {
        if (!host::SettingsUtil::exportToFile(command_line.switchValuePath(u"export"),
                                              command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"update"))
    {
        common::UpdateDialog dialog(
            QString::fromStdU16String(host::SystemSettings().updateServer()), "host");
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else if (command_line.hasSwitch(u"help"))
    {
        // TODO
    }
    else if (command_line.hasSwitch(u"config"))
    {
        if (base::win::isProcessElevated())
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
        else
        {
            LOG(LS_INFO) << "Process not eleavated";
        }
    }
    else
    {
        if (application.isRunning())
        {
            LOG(LS_INFO) << "Application already running";
            application.activate();
        }
        else
        {
            LOG(LS_INFO) << "Application not running yet";

            host::MainWindow window;
            QObject::connect(&application, &host::Application::activated,
                             &window, &host::MainWindow::activateHost);

            if (is_hidden)
            {
                LOG(LS_INFO) << "Hide window to tray";
                window.hideToTray();
            }
            else
            {
                LOG(LS_INFO) << "Show window";
                window.show();
                window.activateWindow();
            }

            window.connectToService();

            return application.exec();
        }
    }

    return 0;
}
