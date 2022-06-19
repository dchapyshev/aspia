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

#include "host/win/service_main.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/scoped_logging.h"
#include "base/sys_info.h"
#include "base/files/base_paths.h"
#include "base/win/mini_dump_writer.h"
#include "base/win/service_controller.h"
#include "base/win/session_info.h"
#include "base/win/session_enumerator.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/service.h"
#include "host/service_constants.h"

#include <iostream>

namespace host {

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

int hostServiceMain(int argc, wchar_t* argv[])
{
    (void)argc;
    (void)argv;

    base::installFailureHandler(L"aspia_host_service");

    base::ScopedLogging scoped_logging;

    base::CommandLine::init(0, nullptr); // On Windows ignores arguments.
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
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

    LOG(LS_INFO) << "Running as user: '" << username << "'";
    LOG(LS_INFO) << "Active console session ID: " << WTSGetActiveConsoleSessionId();
    LOG(LS_INFO) << "Computer name: '" << base::SysInfo::computerName() << "'";

    LOG(LS_INFO) << "Active sessions";
    LOG(LS_INFO) << "#####################################################";
    for (base::win::SessionEnumerator enumerator; !enumerator.isAtEnd(); enumerator.advance())
    {
        LOG(LS_INFO) << enumerator.sessionName() << " (id: " << enumerator.sessionId()
                     << ", host: '" << enumerator.hostName() << "', user: '" << enumerator.userName()
                     << "', domain: '" << enumerator.domainName() << "')";
    }
    LOG(LS_INFO) << "#####################################################";

    LOG(LS_INFO) << "Environment variables";
    LOG(LS_INFO) << "#####################################################";
    for (const auto& variable : base::Environment::list())
    {
        LOG(LS_INFO) << variable.first << ": " << variable.second;
    }
    LOG(LS_INFO) << "#####################################################";

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
                << '\t' << "--version" << '\t' << "Show version information" << std::endl
                << '\t' << "--help" << '\t' << "Show help" << std::endl;
        }
        else
        {
            Service().exec();
        }
    }

    return 0;
}

} // namespace host
