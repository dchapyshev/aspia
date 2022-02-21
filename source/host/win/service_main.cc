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

#include "base/environment.h"
#include "base/scoped_logging.h"
#include "base/sys_info.h"
#include "base/win/mini_dump_writer.h"
#include "base/win/session_info.h"
#include "base/win/session_enumerator.h"
#include "build/version.h"
#include "host/integrity_check.h"
#include "host/win/service.h"

namespace host {

void hostServiceMain()
{
    base::installFailureHandler(L"aspia_host_service");

    base::ScopedLogging scoped_logging;
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

        LOG(LS_INFO) << "Total physical memory: " << (memory_status.ullTotalPhys / kMB) << " MB";
        LOG(LS_INFO) << "Free physical memory: " << (memory_status.ullAvailPhys / kMB) << " MB";

        LOG(LS_INFO) << "Total page file: " << (memory_status.ullTotalPageFile / kMB) << " MB";
        LOG(LS_INFO) << "Free page file: " << (memory_status.ullAvailPageFile / kMB) << " MB";

        LOG(LS_INFO) << "Total virtual memory: " << (memory_status.ullTotalVirtual / kMB) << " MB";
        LOG(LS_INFO) << "Free virtual memory: " << (memory_status.ullAvailVirtual / kMB) << " MB";
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
        Service().exec();
    }
}

} // namespace host
