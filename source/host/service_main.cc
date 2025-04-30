//
// Aspia Project
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

#include "host/service_main.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "base/meta_types.h"
#include "base/sys_info.h"
#include "base/files/base_paths.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/host_key_storage.h"
#include "host/service.h"
#include "host/service_constants.h"
#include "proto/meta_types.h"

#if defined(OS_WIN)
#include "base/win/mini_dump_writer.h"
#include "base/win/service_controller.h"
#include "base/win/session_info.h"
#include "base/win/session_enumerator.h"
#endif // defined(OS_WIN)

#include <iostream>

#include <QProcessEnvironment>

namespace host {

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void startService()
{
    base::win::ServiceController controller =
        base::win::ServiceController::open(host::kHostServiceName);
    if (!controller.isValid())
    {
        std::cout << "Failed to access the service. Not enough rights or service not installed."
                  << std::endl;
    }
    else
    {
        if (!controller.start())
        {
            std::cout << "Failed to start the service." << std::endl;
        }
        else
        {
            std::cout << "The service started successfully." << std::endl;
        }
    }
}
#endif // defined(OS_WIN)

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void stopService()
{
    base::win::ServiceController controller =
        base::win::ServiceController::open(host::kHostServiceName);
    if (!controller.isValid())
    {
        std::cout << "Failed to access the service. Not enough rights or service not installed."
                  << std::endl;
    }
    else
    {
        if (!controller.stop())
        {
            std::cout << "Failed to stop the service." << std::endl;
        }
        else
        {
            std::cout << "The service has stopped successfully." << std::endl;
        }
    }
}
#endif // defined(OS_WIN)

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void installService()
{
    std::filesystem::path file_path;

    if (!base::BasePaths::currentExecFile(&file_path))
    {
        std::cout << "Failed to get the path to the executable." << std::endl;
    }
    else
    {
        base::win::ServiceController controller = base::win::ServiceController::install(
            host::kHostServiceName, host::kHostServiceDisplayName, file_path);
        if (!controller.isValid())
        {
            std::cout << "Failed to install the service." << std::endl;
        }
        else
        {
            controller.setDescription(host::kHostServiceDescription);
            std::cout << "The service has been successfully installed." << std::endl;
        }
    }
}
#endif // defined(OS_WIN)

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void removeService()
{
    if (base::win::ServiceController::isRunning(host::kHostServiceName))
    {
        stopService();
    }

    if (!base::win::ServiceController::remove(host::kHostServiceName))
    {
        std::cout << "Failed to remove the service." << std::endl;
    }
    else
    {
        std::cout << "The service was successfully deleted." << std::endl;
    }
}
#endif // defined(OS_WIN)

//--------------------------------------------------------------------------------------------------
std::optional<QString> currentSessionName()
{
#if defined(OS_WIN)
    DWORD process_session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &process_session_id))
        return std::nullopt;

    DWORD console_session_id = WTSGetActiveConsoleSessionId();
    if (console_session_id == process_session_id)
        return QString();

    base::win::SessionInfo current_session_info(process_session_id);
    if (!current_session_info.isValid())
        return std::nullopt;

    QString user_name = current_session_info.userName().toLower();
    QString domain = current_session_info.domain().toLower();

    if (user_name.isEmpty())
        return std::nullopt;

    using TimeInfo = std::pair<base::SessionId, int64_t>;
    using TimeInfoList = std::vector<TimeInfo>;

    // Enumarate all user sessions.
    TimeInfoList times;
    for (base::win::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (user_name != it.userName().toLower())
            continue;

        base::win::SessionInfo session_info(it.sessionId());
        if (!session_info.isValid())
            continue;

        // In Windows Server can have multiple RDP sessions with the same username. We get a list of
        // sessions with the same username and session connection time.
        times.emplace_back(session_info.sessionId(), session_info.connectTime());
    }

    // Sort the list by the time it was connected to the server.
    std::sort(times.begin(), times.end(), [](const TimeInfo& p1, const TimeInfo& p2)
    {
        return p1.second < p2.second;
    });

    // We are looking for the current session in the sorted list.
    size_t user_number = 0;
    for (size_t i = 0; i < times.size(); ++i)
    {
        if (times[i].first == current_session_info.sessionId())
        {
            // Save the user number in the list.
            user_number = i;
            break;
        }
    }

    // The session name contains the username and domain to reliably distinguish between local and
    // domain users. It also contains the user number found above. This way, users will receive the
    // same ID based on the time they were connected to the server.
    return user_name + "@" + domain + "@" + QString::number(user_number);
#else
    return std::nullopt;
#endif
}

#if defined(OS_LINUX)
//--------------------------------------------------------------------------------------------------
int hostServiceMain(int& argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING << " (arch: " << ARCH_CPU_STRING << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

    LOG(LS_INFO) << "Environment variables";
    LOG(LS_INFO) << "#####################################################";
    for (const auto& variable : base::Environment::list())
    {
        LOG(LS_INFO) << variable.first << ": " << variable.second;
    }
    LOG(LS_INFO) << "#####################################################";

    if (command_line->hasSwitch(u"version"))
    {
        std::cout << ASPIA_VERSION_MAJOR << "." << ASPIA_VERSION_MINOR << "."
                  << ASPIA_VERSION_PATCH << "." << GIT_COMMIT_COUNT << std::endl;
    }
    else if (command_line->hasSwitch(u"host-id"))
    {
        std::optional<std::string> session_name = currentSessionName();
        if (!session_name.has_value())
            return 0;

        HostKeyStorage storage;
        std::cout << storage.lastHostId(*session_name) << std::endl;
    }
    else if (command_line->hasSwitch(u"help"))
    {
        std::cout << "aspia_host_service [switch]" << std::endl
                  << "Available switches:" << std::endl
                  << '\t' << "--host-id" << '\t' << "Get current host id" << std::endl
                  << '\t' << "--version" << '\t' << "Show version information" << std::endl
                  << '\t' << "--help" << '\t' << "Show help" << std::endl;
    }
    else
    {
        Service().exec(argc, argv);
    }

    return 0;
}
#endif // defined(OS_LINUX)

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
int hostServiceMain(int& argc, char* argv[])
{
    (void)argc;
    (void)argv;

    base::installFailureHandler(L"aspia_host_service");

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::CommandLine::init(0, nullptr); // On Windows ignores arguments.
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING << " (arch: " << ARCH_CPU_STRING << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

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
        PLOG(LS_ERROR) << "GlobalMemoryStatusEx failed";
    }

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
    }
    else
    {
        base::win::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_ERROR) << "Unable to get session info";
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
        PLOG(LS_ERROR) << "GetUserNameW failed";
    }

    LOG(LS_INFO) << "Running as user: '" << username << "'";
    LOG(LS_INFO) << "Active console session ID: " << WTSGetActiveConsoleSessionId();
    LOG(LS_INFO) << "Computer name: '" << base::SysInfo::computerName() << "'";

    LOG(LS_INFO) << "Active sessions";
    LOG(LS_INFO) << "#####################################################";
    for (base::win::SessionEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        LOG(LS_INFO) << enumerator.sessionName() << " (id=" << enumerator.sessionId()
                     << " host='" << enumerator.hostName() << "', user='" << enumerator.userName()
                     << "', domain='" << enumerator.domainName() << "', locked="
                     << enumerator.isUserLocked() << ")";
    }
    LOG(LS_INFO) << "#####################################################";

    LOG(LS_INFO) << "Environment variables: " << QProcessEnvironment::systemEnvironment().toStringList();

    if (!integrityCheck())
    {
        LOG(LS_ERROR) << "Integrity check failed. Application stopped";
    }
    else
    {
        LOG(LS_INFO) << "Integrity check passed successfully";

        if (command_line->hasSwitch(u"version"))
        {
            std::cout << ASPIA_VERSION_MAJOR << "." << ASPIA_VERSION_MINOR << "."
                      << ASPIA_VERSION_PATCH << "." << GIT_COMMIT_COUNT << std::endl;
        }
        else if (command_line->hasSwitch(u"host-id"))
        {
            std::optional<QString> session_name = currentSessionName();
            if (!session_name.has_value())
                return 0;

            HostKeyStorage storage;
            std::cout << storage.lastHostId(*session_name) << std::endl;
        }
        else if (command_line->hasSwitch(u"install"))
        {
            installService();
        }
        else if (command_line->hasSwitch(u"remove"))
        {
            removeService();
        }
        else if (command_line->hasSwitch(u"start"))
        {
            startService();
        }
        else if (command_line->hasSwitch(u"stop"))
        {
            stopService();
        }
        else if (command_line->hasSwitch(u"help"))
        {
            std::cout << "aspia_host_service [switch]" << std::endl
                << "Available switches:" << std::endl
                << '\t' << "--install" << '\t' << "Install service" << std::endl
                << '\t' << "--remove" << '\t' << "Remove service" << std::endl
                << '\t' << "--start" << '\t' << "Start service" << std::endl
                << '\t' << "--stop" << '\t' << "Stop service" << std::endl
                << '\t' << "--host-id" << '\t' << "Get current host id" << std::endl
                << '\t' << "--version" << '\t' << "Show version information" << std::endl
                << '\t' << "--help" << '\t' << "Show help" << std::endl;
        }
        else
        {
            base::registerMetaTypes();
            proto::registerMetaTypes();

            Service().exec(argc, argv);
        }
    }

    return 0;
}
#endif // defined(OS_WIN)

} // namespace host
