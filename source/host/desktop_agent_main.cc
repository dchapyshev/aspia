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

#include "host/desktop_agent_main.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/scoped_logging.h"
#include "base/sys_info.h"
#include "base/message_loop/message_loop.h"
#include "base/win/mini_dump_writer.h"
#include "base/win/session_info.h"
#include "build/version.h"
#include "host/desktop_session_agent.h"

void desktopAgentMain(int argc, const char* const* argv)
{
    base::installFailureHandler(L"aspia_desktop_agent");

    base::ScopedLogging scoped_logging;

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "Command line: " << command_line->commandLineString();
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

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
            LOG(LS_INFO) << "Session ID: " << session_id;
            LOG(LS_INFO) << "Running in user session: " << session_info.userName();
            LOG(LS_INFO) << "Session connect state: "
                << base::win::SessionInfo::connectStateToString(session_info.connectState());
            LOG(LS_INFO) << "WinStation name: " << session_info.winStationName();
            LOG(LS_INFO) << "Domain name: " << session_info.domain();
        }
    }

    wchar_t username[64] = { 0 };
    DWORD username_size = sizeof(username) / sizeof(username[0]);
    if (!GetUserNameW(username, &username_size))
    {
        PLOG(LS_WARNING) << "GetUserNameW failed";
    }

    LOG(LS_INFO) << "Running as user: " << username;
    LOG(LS_INFO) << "Active console session ID: " << WTSGetActiveConsoleSessionId();
    LOG(LS_INFO) << "Computer name: " << base::SysInfo::computerName();

    LOG(LS_INFO) << "Environment variables";
    LOG(LS_INFO) << "#####################################################";
    for (const auto& variable : base::Environment::list())
    {
        LOG(LS_INFO) << variable.first << ": " << variable.second;
    }
    LOG(LS_INFO) << "#####################################################";

    if (command_line->hasSwitch(u"channel_id"))
    {
        std::unique_ptr<base::MessageLoop> message_loop =
            std::make_unique<base::MessageLoop>(base::MessageLoop::Type::ASIO);

        std::shared_ptr<host::DesktopSessionAgent> desktop_agent =
            std::make_shared<host::DesktopSessionAgent>(message_loop->taskRunner());

        desktop_agent->start(command_line->switchValue(u"channel_id"));
        message_loop->run();
    }
    else
    {
        LOG(LS_ERROR) << "Parameter channel_id is not specified";
    }
}
