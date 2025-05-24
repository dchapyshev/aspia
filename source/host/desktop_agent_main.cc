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

#include "host/desktop_agent_main.h"

#include "base/application.h"
#include "base/asio_event_dispatcher.h"
#include "base/meta_types.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "build/version.h"
#include "host/desktop_session_agent.h"
#include "proto/meta_types.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/desktop.h"
#include "base/win/session_info.h"
#include "base/win/window_station.h"

#include <dxgi.h>
#include <d3d11.h>
#include <comdef.h>
#include <wrl/client.h>
#endif // defined(Q_OS_WINDOWS)

#include <QCommandLineParser>
#include <QProcessEnvironment>
#include <QSysInfo>

namespace {

//--------------------------------------------------------------------------------------------------
void printDebugInfo()
{
    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING
                 << " (arch: " << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "Command line: " << base::Application::arguments();
    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "CPU: " << base::SysInfo::processorName()
                 << " (vendor: " << base::SysInfo::processorVendor()
                 << " packages: " << base::SysInfo::processorPackages()
                 << " cores: " << base::SysInfo::processorCores()
                 << " threads: " << base::SysInfo::processorThreads() << ")";

#if defined(Q_OS_WINDOWS)
    MEMORYSTATUSEX memory_status;
    memset(&memory_status, 0, sizeof(memory_status));
    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status))
    {
        static const quint32 kMB = 1024 * 1024;

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

    LOG(LS_INFO) << "Video adapters";
    LOG(LS_INFO) << "#####################################################";
    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    _com_error error = CreateDXGIFactory(__uuidof(IDXGIFactory),
                                         reinterpret_cast<void**>(factory.GetAddressOf()));
    if (error.Error() != S_OK || !factory)
    {
        LOG(LS_INFO) << "CreateDXGIFactory failed: " << error.ErrorMessage();
    }
    else
    {
        UINT adapter_index = 0;

        while (true)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
            error = factory->EnumAdapters(adapter_index, adapter.GetAddressOf());
            if (error.Error() != S_OK)
                break;

            DXGI_ADAPTER_DESC adapter_desc;
            memset(&adapter_desc, 0, sizeof(adapter_desc));

            if (SUCCEEDED(adapter->GetDesc(&adapter_desc)))
            {
                static const quint32 kMB = 1024 * 1024;

                LOG(LS_INFO) << adapter_desc.Description << " ("
                             << "video memory: " << (adapter_desc.DedicatedVideoMemory / kMB) << "MB"
                             << ", system memory: " << (adapter_desc.DedicatedSystemMemory / kMB) << "MB"
                             << ", shared memory: " << (adapter_desc.SharedSystemMemory / kMB) << "MB"
                             << ")";
            }

            ++adapter_index;
        }
    }
    LOG(LS_INFO) << "#####################################################";

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(LS_ERROR) << "ProcessIdToSessionId failed";
    }
    else
    {
        base::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(LS_ERROR) << "Unable to get session info";
        }
        else
        {
            LOG(LS_INFO) << "Process session ID: " << session_id;
            LOG(LS_INFO) << "Running in user session: '" << session_info.userName() << "'";
            LOG(LS_INFO) << "Session connect state: "
                         << base::SessionInfo::connectStateToString(session_info.connectState());
            LOG(LS_INFO) << "WinStation name: '" << session_info.winStationName() << "'";
            LOG(LS_INFO) << "Domain name: '" << session_info.domain() << "'";
            LOG(LS_INFO) << "User Locked: " << session_info.isUserLocked();
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
    LOG(LS_INFO) << "Process WindowStation: " << base::WindowStation::forCurrentProcess().name();

    LOG(LS_INFO) << "WindowStation list";
    LOG(LS_INFO) << "#####################################################";
    QStringList windowStations = base::WindowStation::windowStationList();
    for (const auto& window_station_name : std::as_const(windowStations))
    {
        QString desktops;

        base::WindowStation window_station = base::WindowStation::open(window_station_name);
        if (window_station.isValid())
        {
            QStringList list = base::Desktop::desktopList(window_station.get());

            for (int i = 0; i < list.size(); ++i)
            {
                desktops += list[i];
                if ((i + 1) != list.size())
                    desktops += ", ";
            }
        }

        LOG(LS_INFO) << window_station_name << " (desktops: " << desktops << ")";
    }
    LOG(LS_INFO) << "#####################################################";
    LOG(LS_INFO) << "Environment variables: " << QProcessEnvironment::systemEnvironment().toStringList();
#endif // defined(Q_OS_WINDOWS)
}

} // namespace

//--------------------------------------------------------------------------------------------------
int desktopAgentMain(int& argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

    printDebugInfo();

    QCommandLineOption channel_id_option("channel_id",
        base::Application::translate("FileAgentMain", "IPC channel id."), "channel_id");

    QCommandLineParser parser;
    parser.addOption(channel_id_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    if (!parser.isSet(channel_id_option))
    {
        LOG(LS_ERROR) << "Parameter channel_id is not specified";
        return 1;
    }

    base::registerMetaTypes();
    proto::registerMetaTypes();

    host::DesktopSessionAgent desktop_agent;
    desktop_agent.start(parser.value(channel_id_option));

    return application.exec();
}
