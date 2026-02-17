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

#include "host/print_debug_info.h"

#include <QProcessEnvironment>

#if defined(Q_OS_WINDOWS)
#include "base/win/desktop.h"
#include "base/win/process_util.h"
#include "base/win/session_info.h"
#include "base/win/window_station.h"

#include <dxgi.h>
#include <d3d11.h>
#include <comdef.h>
#include <wrl/client.h>
#endif // defined(Q_OS_WINDOWS)

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "build/version.h"

namespace host {

void printDebugInfo(quint32 features)
{
    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING
              << " (arch: " << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(INFO) << "Git branch:" << GIT_CURRENT_BRANCH;
    LOG(INFO) << "Git commit:" << GIT_COMMIT_HASH;
#endif
    LOG(INFO) << "Qt version:" << QT_VERSION_STR;
    LOG(INFO) << "Command line:" << base::GuiApplication::arguments();
    LOG(INFO) << "OS:" << base::SysInfo::operatingSystemName()
              << "(version:" << base::SysInfo::operatingSystemVersion()
              <<  "arch:" << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(INFO) << "CPU:" << base::SysInfo::processorName()
              << "(vendor:" << base::SysInfo::processorVendor()
              << "packages:" << base::SysInfo::processorPackages()
              << "cores:" << base::SysInfo::processorCores()
              << "threads:" << base::SysInfo::processorThreads() << ")";

#if defined(Q_OS_WINDOWS)
    MEMORYSTATUSEX memory_status;
    memset(&memory_status, 0, sizeof(memory_status));
    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status))
    {
        static const quint32 kMB = 1024 * 1024;

        LOG(INFO) << "Total physical memory:" << (memory_status.ullTotalPhys / kMB)
                  << "MB (free:" << (memory_status.ullAvailPhys / kMB) << "MB)";
        LOG(INFO) << "Total page file:" << (memory_status.ullTotalPageFile / kMB)
                  << "MB (free:" << (memory_status.ullAvailPageFile / kMB) << "MB)";
        LOG(INFO) << "Total virtual memory:" << (memory_status.ullTotalVirtual / kMB)
                  << "MB (free:" << (memory_status.ullAvailVirtual / kMB) << "MB)";
    }
    else
    {
        PLOG(ERROR) << "GlobalMemoryStatusEx failed";
    }

    DWORD session_id = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        PLOG(ERROR) << "ProcessIdToSessionId failed";
    }
    else
    {
        base::SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(ERROR) << "Unable to get session info";
        }
        else
        {
            LOG(INFO) << "Process session ID:" << session_id;
            LOG(INFO) << "Running in user session:" << session_info.userName();
            LOG(INFO) << "Session connect state:" << session_info.connectState();
            LOG(INFO) << "WinStation name:" << session_info.winStationName();
            LOG(INFO) << "Domain name:" << session_info.domain();
            LOG(INFO) << "User Locked:" << session_info.isUserLocked();
        }
    }

    wchar_t username[64] = { 0 };
    DWORD username_size = sizeof(username) / sizeof(username[0]);
    if (!GetUserNameW(username, &username_size))
    {
        PLOG(ERROR) << "GetUserNameW failed";
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
            PLOG(ERROR) << "CheckTokenMembership failed";
            is_user_admin = FALSE;
        }
        FreeSid(admins_group);
    }
    else
    {
        PLOG(ERROR) << "AllocateAndInitializeSid failed";
    }

    LOG(INFO) << "Running as user:" << username;
    LOG(INFO) << "Member of admins group:" << (is_user_admin ? "Yes" : "No");
    LOG(INFO) << "Process elevated:" << (base::isProcessElevated() ? "Yes" : "No");
    LOG(INFO) << "Active console session ID:" << WTSGetActiveConsoleSessionId();
    LOG(INFO) << "Computer name:" << base::SysInfo::computerName();

    if (features & INCLUDE_VIDEO_ADAPTERS)
    {
        LOG(INFO) << "Video adapters";
        LOG(INFO) << "#####################################################";
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;
        _com_error error = CreateDXGIFactory(__uuidof(IDXGIFactory),
                                             reinterpret_cast<void**>(factory.GetAddressOf()));
        if (error.Error() != S_OK || !factory)
        {
            LOG(INFO) << "CreateDXGIFactory failed:" << error.ErrorMessage();
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

                    LOG(INFO) << adapter_desc.Description << "("
                              << "video memory:" << (adapter_desc.DedicatedVideoMemory / kMB) << "MB"
                              << ", system memory:" << (adapter_desc.DedicatedSystemMemory / kMB) << "MB"
                              << ", shared memory:" << (adapter_desc.SharedSystemMemory / kMB) << "MB"
                              << ")";
                }

                ++adapter_index;
            }
        }
        LOG(INFO) << "#####################################################";
    }

    if (features & INCLUDE_WINDOW_STATIONS)
    {
        LOG(INFO) << "WindowStation list";
        LOG(INFO) << "#####################################################";
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

            LOG(INFO) << window_station_name << "(desktops:" << desktops << ")";
        }
        LOG(INFO) << "#####################################################";
    }
#endif // defined(Q_OS_WINDOWS)

    LOG(INFO) << "Environment variables:" << QProcessEnvironment::systemEnvironment().toStringList();
}

} // namespace host
