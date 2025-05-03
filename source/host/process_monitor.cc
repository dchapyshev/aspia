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

#include "host/process_monitor.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/scoped_object.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"

#include <memory>

#pragma warning(push)
#pragma warning(disable : 4005)
#include <ntstatus.h>
#include <winternl.h>
#include <Psapi.h>
#pragma warning(pop)

namespace host {

namespace {

typedef struct {
    ULONG Reserved;
    ULONG TimerResolution;
    ULONG PageSize;
    ULONG NumberOfPhysicalPages;
    ULONG LowestPhysicalPageNumber;
    ULONG HighestPhysicalPageNumber;
    ULONG AllocationGranularity;
    ULONG_PTR MinimumUserModeAddress;
    ULONG_PTR MaximumUserModeAddress;
    KAFFINITY ActiveProcessorAffinityMask;
    CCHAR NumberOfProcessors;
} OWN_SYSTEM_BASIC_INFORMATION;

typedef struct {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LONGLONG WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG Reserved1;
    LONGLONG CycleTime;
    LONGLONG CreateTime;
    LONGLONG UserTime;
    LONGLONG KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR PageDirectoryBase;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LONGLONG ReadOperationCount;
    LONGLONG WriteOperationCount;
    LONGLONG OtherOperationCount;
    LONGLONG ReadTransferCount;
    LONGLONG WriteTransferCount;
    LONGLONG OtherTransferCount;
} OWN_SYSTEM_PROCESS_INFORMATION;

typedef NTSTATUS (NTAPI* NtQuerySystemInformationFunc)
    (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

class ScopedPrivilege
{
public:
    ScopedPrivilege(const std::wstring& name)
        : name_(name)
    {
        base::ScopedHandle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.recieve()))
        {
            PLOG(LS_ERROR) << "OpenProcessToken failed";
            return;
        }

        setPrivilege(token, name_.c_str(), TRUE);
    }

    ~ScopedPrivilege()
    {
        base::ScopedHandle token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.recieve()))
        {
            PLOG(LS_ERROR) << "OpenProcessToken failed";
            return;
        }

        setPrivilege(token, name_.c_str(), FALSE);
    }

private:
    bool setPrivilege(HANDLE token, LPCTSTR privilege, BOOL enable)
    {
        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, privilege, &luid))
        {
            PLOG(LS_ERROR) << "LookupPrivilegeValueW failed";
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        if (enable)
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        else
            tp.Privileges[0].Attributes = 0;

        if (!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
        {
            PLOG(LS_ERROR) << "AdjustTokenPrivileges failed";
            return false;
        }

        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        {
            LOG(LS_ERROR) << "The token does not have the specified privilege";
            return false;
        }

        return true;
    }

    const std::wstring name_;

    DISALLOW_COPY_AND_ASSIGN(ScopedPrivilege);
};

//--------------------------------------------------------------------------------------------------
std::string userNameByHandle(HANDLE process)
{
    base::ScopedHandle token;
    if (!OpenProcessToken(process, TOKEN_QUERY, token.recieve()))
    {
        PLOG(LS_ERROR) << "OpenProcessToken failed";
        return std::string();
    }

    std::unique_ptr<uint8_t[]> token_user_buffer;
    TOKEN_USER* token_user = nullptr;
    DWORD length = 0;

    if (!GetTokenInformation(token, TokenUser, token_user, 0, &length))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            PLOG(LS_ERROR) << "GetTokenInformation failed";
            return std::string();
        }

        token_user_buffer = std::make_unique<uint8_t[]>(length);
        memset(token_user_buffer.get(), 0, sizeof(length));

        token_user = reinterpret_cast<TOKEN_USER*>(token_user_buffer.get());
    }

    if (!token_user)
    {
        LOG(LS_ERROR) << "Invalid user token buffer";
        return std::string();
    }

    if (!GetTokenInformation(token, TokenUser, token_user, length, &length))
    {
        PLOG(LS_ERROR) << "GetTokenInformation failed";
        return std::string();
    }

    wchar_t domain_buffer[128] = {0};
    wchar_t user_buffer[128] = {0};
    DWORD user_buffer_size = static_cast<DWORD>(std::size(user_buffer));
    DWORD domain_buffer_size = static_cast<DWORD>(std::size(domain_buffer));
    SID_NAME_USE sid_type;

    if (!LookupAccountSidW(nullptr, token_user->User.Sid,
                           user_buffer, &user_buffer_size,
                           domain_buffer, &domain_buffer_size,
                           &sid_type))
    {
        PLOG(LS_ERROR) << "LookupAccountSidW failed";
        return std::string();
    }

    return base::utf8FromWide(user_buffer);
}

//--------------------------------------------------------------------------------------------------
std::string filePathByHandle(HANDLE process)
{
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD buffer_size = static_cast<DWORD>(std::size(buffer));

    if (!QueryFullProcessImageNameW(process, 0, buffer, &buffer_size))
    {
        LOG(LS_ERROR) << "QueryFullProcessImageNameW failed";
        return std::string();
    }

    return base::utf8FromWide(buffer);
}

//--------------------------------------------------------------------------------------------------
int32_t calcCpuRatio(int64_t cpu_time_delta, int64_t total_time)
{
    int64_t cpu_ratio =
        (((cpu_time_delta / ((total_time / 1000LL) ? (total_time / 1000LL) : 1) + 5LL)) / 10LL);
    if (cpu_ratio > 99)
        cpu_ratio = 99;
    return static_cast<int32_t>(cpu_ratio);
}

//--------------------------------------------------------------------------------------------------
void updateProcess(ProcessMonitor::ProcessEntry* entry, const OWN_SYSTEM_PROCESS_INFORMATION& info,
                   int64_t total_time, bool update_only)
{
    int64_t time = info.KernelTime + info.UserTime;
    int64_t time_delta = time - entry->cpu_time;

    if (time_delta)
        entry->cpu_time = time;

    entry->cpu_ratio               = calcCpuRatio(time_delta, total_time);
    entry->session_id              = info.SessionId;
    entry->mem_working_set_delta   = info.WorkingSetSize - entry->mem_working_set;
    entry->mem_private_working_set = info.WorkingSetPrivateSize;
    entry->mem_working_set         = info.WorkingSetSize;
    entry->mem_peak_working_set    = info.PeakWorkingSetSize;
    entry->thread_count            = info.NumberOfThreads;

    entry->process_name_changed = false;
    entry->user_name_changed = false;
    entry->file_path_changed = false;

    if (!update_only)
    {
        if (info.ImageName.Buffer)
        {
            entry->process_name = base::utf8FromWide(info.ImageName.Buffer);
            entry->process_name_changed = true;
        }

        uint32_t process_id = PtrToUlong(info.UniqueProcessId);
        if (process_id != 0)
        {
            base::ScopedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
            if (!process.isValid())
            {
                PLOG(LS_ERROR) << "OpenProcess failed";
            }
            else
            {
                entry->user_name = userNameByHandle(process);
                entry->user_name_changed = true;

                entry->file_path = filePathByHandle(process);
                entry->file_path_changed = true;
            }
        }
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
ProcessMonitor::ProcessMonitor()
{
    LOG(LS_INFO) << "Ctor";

    memset(&prev_cpu_idle_time_, 0, sizeof(prev_cpu_idle_time_));
    memset(&prev_cpu_total_time_, 0, sizeof(prev_cpu_total_time_));

    ntdll_library_ = LoadLibraryW(L"ntdll.dll");
    if (!ntdll_library_)
    {
        PLOG(LS_ERROR) << "LoadLibraryW failed";
        return;
    }

    nt_query_system_info_func_ = reinterpret_cast<void*>(
        GetProcAddress(reinterpret_cast<HMODULE>(ntdll_library_), "NtQuerySystemInformation"));
    if (!nt_query_system_info_func_)
    {
        PLOG(LS_ERROR) << "GetProcAddress failed";
        return;
    }

    NtQuerySystemInformationFunc nt_query_system_information_func =
        reinterpret_cast<NtQuerySystemInformationFunc>(nt_query_system_info_func_);

    OWN_SYSTEM_BASIC_INFORMATION basic_info;
    NTSTATUS status = nt_query_system_information_func(
        SystemBasicInformation, &basic_info, sizeof(basic_info), nullptr);
    if (!NT_SUCCESS(status))
    {
        LOG(LS_ERROR) << "NtQuerySystemInformation failed: " << status;
    }
    else
    {
        processor_count_ = static_cast<uint32_t>(basic_info.NumberOfProcessors);
        if (processor_count_ > kMaxCpuCount)
            processor_count_ = kMaxCpuCount;

        SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION processor_info[kMaxCpuCount];
        status = nt_query_system_information_func(
           SystemProcessorPerformanceInformation,
           processor_info,
           sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * kMaxCpuCount,
           nullptr);
        if (!NT_SUCCESS(status))
        {
            LOG(LS_ERROR) << "NtQuerySystemInformation failed: " << status;
        }
        else
        {
            for (uint32_t i = 0; i < processor_count_; ++i)
            {
                const SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION& current = processor_info[i];

                prev_cpu_idle_time_[i] = current.IdleTime.QuadPart;
                prev_cpu_total_time_[i] = current.UserTime.QuadPart + current.KernelTime.QuadPart;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
ProcessMonitor::~ProcessMonitor()
{
    LOG(LS_INFO) << "Dtor";

    if (ntdll_library_)
        FreeLibrary(reinterpret_cast<HMODULE>(ntdll_library_));
}

//--------------------------------------------------------------------------------------------------
const ProcessMonitor::ProcessMap& ProcessMonitor::processes(bool reset_cache)
{
    if (reset_cache)
        table_.clear();

    if (nt_query_system_info_func_)
    {
        if (updateSnapshot())
            updateTable();
    }

    return table_;
}

//--------------------------------------------------------------------------------------------------
int ProcessMonitor::calcCpuUsage()
{
    NtQuerySystemInformationFunc nt_query_system_information_func =
        reinterpret_cast<NtQuerySystemInformationFunc>(nt_query_system_info_func_);
    if (!nt_query_system_information_func)
        return 0;

    SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION processor_info[kMaxCpuCount];

    NTSTATUS status = nt_query_system_information_func(
       SystemProcessorPerformanceInformation,
       processor_info,
       sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * kMaxCpuCount,
       nullptr);
    if (!NT_SUCCESS(status))
    {
        LOG(LS_ERROR) << "NtQuerySystemInformation failed: " << status;
        return 0;
    }

    int64_t cpu_idle_time[kMaxCpuCount];
    int64_t cpu_total_time[kMaxCpuCount];

    int64_t summary_idle_time = 0;
    int64_t summary_total_time = 0;

    for (uint32_t i = 0; i < processor_count_; ++i)
    {
        const SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION& current = processor_info[i];

        cpu_idle_time[i] = current.IdleTime.QuadPart;
        cpu_total_time[i] = current.KernelTime.QuadPart + current.UserTime.QuadPart;

        int64_t delta_cpu_idle_time = cpu_idle_time[i] - prev_cpu_idle_time_[i];
        int64_t delta_cpu_total_time = cpu_total_time[i] - prev_cpu_total_time_[i];

        summary_idle_time += delta_cpu_idle_time;
        summary_total_time += delta_cpu_total_time;

        prev_cpu_idle_time_[i] = cpu_idle_time[i];
        prev_cpu_total_time_[i] = cpu_total_time[i];
    }

    int current_cpu_usage = 0;
    if (summary_total_time)
    {
        current_cpu_usage = static_cast<int>(
            100 - ((summary_idle_time * 100LL) / summary_total_time));
    }

    return current_cpu_usage;
}

//--------------------------------------------------------------------------------------------------
int ProcessMonitor::calcMemoryUsage()
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status))
    {
        PLOG(LS_ERROR) << "GlobalMemoryStatusEx failed";
        return 0;
    }

    return status.dwMemoryLoad;
}

//--------------------------------------------------------------------------------------------------
bool ProcessMonitor::endProcess(ProcessId process_id)
{
    auto result = table_.find(process_id);
    if (result == table_.end())
    {
        LOG(LS_ERROR) << "Process not found in table";
        return false;
    }

    static const char16_t* kBlackList[] =
        { u"services.exe", u"lsass.exe", u"smss.exe", u"winlogon.exe", u"csrss.exe" };

    std::u16string process_name = base::utf16FromUtf8(result->second.process_name);

    for (size_t i = 0; i < std::size(kBlackList); ++i)
    {
        if (base::compareCaseInsensitive(process_name, kBlackList[i]) == 0)
        {
            LOG(LS_ERROR) << "Unable to end system process";
            return false;
        }
    }

    ScopedPrivilege debug_privilege(SE_DEBUG_NAME);

    base::ScopedHandle process(OpenProcess(PROCESS_TERMINATE, FALSE, process_id));
    if (!process.isValid())
    {
        PLOG(LS_ERROR) << "OpenProcess failed";
        return false;
    }

    if (!TerminateProcess(process, 1))
    {
        PLOG(LS_ERROR) << "TerminateProcess failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ProcessMonitor::updateSnapshot()
{
    while (true)
    {
        if (!snapshot_.empty())
        {
            NtQuerySystemInformationFunc nt_query_system_information_func =
                reinterpret_cast<NtQuerySystemInformationFunc>(nt_query_system_info_func_);

            NTSTATUS status = nt_query_system_information_func(SystemProcessInformation,
                snapshot_.data(), static_cast<DWORD>(snapshot_.size()), nullptr);
            if (NT_SUCCESS(status))
                return true;

            if (status != STATUS_INFO_LENGTH_MISMATCH)
            {
                LOG(LS_ERROR) << "NtQuerySystemInformation failed: " << status;
                return false;
            }
        }

        static const size_t kGrowSize = 4096;
        snapshot_.resize(snapshot_.size() + kGrowSize);
    }
}

//--------------------------------------------------------------------------------------------------
void ProcessMonitor::updateTable()
{
    OWN_SYSTEM_PROCESS_INFORMATION* current;
    int64_t last_total_time = 0;
    int64_t total_time = 0;

    // Calculate CPU times.
    size_t offset = 0;
    do
    {
        current = reinterpret_cast<OWN_SYSTEM_PROCESS_INFORMATION*>(&snapshot_[offset]);

        if (current->UniqueProcessId || current->NumberOfThreads)
        {
            auto old_info = table_.find(PtrToUlong(current->UniqueProcessId));
            if (old_info != table_.end())
            {
                if (old_info->second.cpu_time > current->KernelTime + current->UserTime)
                {
                    // PID has been reused.
                    table_.erase(old_info);
                }
                else if (!current->UniqueProcessId && !current->KernelTime && !current->UserTime)
                {
                    table_.erase(old_info);
                }
                else
                {
                    last_total_time += old_info->second.cpu_time;
                }
            }

            total_time += current->KernelTime + current->UserTime;
        }

        offset += current->NextEntryOffset;
    }
    while (current->NextEntryOffset);

    int64_t time_delta = total_time - last_total_time;
    std::set<ProcessId> active_pids;

    // Update process list.
    offset = 0;
    do
    {
        current = reinterpret_cast<OWN_SYSTEM_PROCESS_INFORMATION*>(&snapshot_[offset]);

        if (current->UniqueProcessId || current->NumberOfThreads)
        {
            ProcessId process_id = PtrToUlong(current->UniqueProcessId);

            auto old_info = table_.find(process_id);
            if (old_info != table_.end())
            {
                updateProcess(&old_info->second, *current, time_delta, true);
            }
            else
            {
                ProcessEntry entry;
                updateProcess(&entry, *current, time_delta, false);
                table_.emplace(process_id, std::move(entry));
            }

            active_pids.emplace(process_id);
        }

        offset += current->NextEntryOffset;
    }
    while (current->NextEntryOffset);

    // Remove obsolete processes.
    for (auto it = table_.begin(); it != table_.end();)
    {
        if (!base::contains(active_pids, it->first))
            it = table_.erase(it);
        else
            ++it;
    }
}

} // namespace host
