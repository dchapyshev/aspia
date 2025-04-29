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

#include "base/win/safe_mode_util.h"

#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/scoped_object.h"

#include <strsafe.h>

namespace base::win {

namespace {

class ScopedWow64FsRedirection
{
public:
    ScopedWow64FsRedirection();
    ~ScopedWow64FsRedirection();

    bool isSucceeded() const { return succeeded_; }

private:
    bool required_ = false;
    bool succeeded_ = false;
    void* old_value_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedWow64FsRedirection);
};

//--------------------------------------------------------------------------------------------------
ScopedWow64FsRedirection::ScopedWow64FsRedirection()
{
    BOOL wow64 = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &wow64))
    {
        PLOG(LS_ERROR) << "IsWow64Process failed";
        return;
    }

    required_ = wow64 != FALSE;
    if (!required_)
    {
        // Disabling redirection is not required. An x86 process runs on an x86 system or an x86_64
        // process runs on an x86_64 system.
        succeeded_ = true;
        return;
    }

    if (!Wow64DisableWow64FsRedirection(&old_value_))
    {
        PLOG(LS_ERROR) << "Wow64DisableWow64FsRedirection failed";
        return;
    }

    // Redirection has been successfully disabled.
    succeeded_ = true;
}

//--------------------------------------------------------------------------------------------------
ScopedWow64FsRedirection::~ScopedWow64FsRedirection()
{
    if (required_ && succeeded_)
    {
        if (!Wow64RevertWow64FsRedirection(old_value_))
        {
            PLOG(LS_ERROR) << "Wow64RevertWow64FsRedirection";
        }
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool SafeModeUtil::setSafeMode(bool enable)
{
    static const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

    ScopedWow64FsRedirection wow64_fs_redirection;
    if (!wow64_fs_redirection.isSucceeded())
    {
        LOG(LS_ERROR) << "Failed to disable file system redirection";
        return false;
    }

    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    wchar_t command_line[MAX_PATH] = { 0 };

    if (enable)
    {
        // Turn on Safe Mode with Networking.
        StringCbCopyW(command_line, sizeof(command_line), L"bcdedit.exe /set safeboot network");
    }
    else
    {
        // Turn off Safe mode.
        StringCbCopyW(command_line, sizeof(command_line), L"bcdedit.exe /deletevalue safeboot");
    }

    if (!CreateProcessW(nullptr,
                        command_line,
                        nullptr,
                        nullptr,
                        FALSE,
                        NORMAL_PRIORITY_CLASS,
                        nullptr,
                        nullptr,
                        &startup_info,
                        &process_info))
    {
        PLOG(LS_ERROR) << "CreateProcessW failed";
        return false;
    }

    ScopedHandle process_deleter(process_info.hProcess);
    ScopedHandle thread_deleter(process_info.hThread);

    DWORD ret = WaitForSingleObject(process_info.hProcess, 15000);
    switch (ret)
    {
        case WAIT_OBJECT_0:
            // Process has ended.
            break;

        case WAIT_TIMEOUT:
            LOG(LS_ERROR) << "Process did not complete its work within the specified time";
            return false;

        default:
            LOG(LS_ERROR) << "An error occurred while waiting for the process to complete: " << ret;
            return false;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code))
    {
        PLOG(LS_ERROR) << "GetExitCodeProcess failed";
        return false;
    }

    if (exit_code != 0)
    {
        LOG(LS_ERROR) << "Process ended with exit code: " << exit_code;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool SafeModeUtil::setSafeModeService(const QString& service_name, bool enable)
{
    QString key_path = "SYSTEM\\CurrentControlSet\\Control\\SafeBoot\\Network\\";
    key_path.append(service_name);

    if (enable)
    {
        RegistryKey key;
        LONG status = key.create(HKEY_LOCAL_MACHINE,
                                 reinterpret_cast<const wchar_t*>(key_path.utf16()),
                                 KEY_READ | KEY_WRITE);
        if (status != ERROR_SUCCESS)
        {
            LOG(LS_ERROR) << "create failed: "
                          << base::SystemError::toString(static_cast<ULONG>(status));
            return false;
        }

        status = key.writeValue(L"", L"Service");
        if (status != ERROR_SUCCESS)
        {
            LOG(LS_ERROR) << "writeValue failed: "
                          << base::SystemError::toString(static_cast<ULONG>(status));
        }
    }
    else
    {
        LONG status = RegDeleteTreeW(HKEY_LOCAL_MACHINE,
                                     reinterpret_cast<const wchar_t*>(key_path.utf16()));
        if (status == ERROR_FILE_NOT_FOUND)
        {
            // Service is no longer listed to boot in Safe Mode.
            return true;
        }

        if (status != ERROR_SUCCESS)
        {
            LOG(LS_ERROR) << "RegDeleteTreeW failed: "
                          << base::SystemError::toString(static_cast<ULONG>(status));
            return false;
        }
    }

    return true;
}

} // namespace base::win
