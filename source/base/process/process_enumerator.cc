//
// PROJECT:         Aspia
// FILE:            base/process/process_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/scoped_privilege.h"
#include "base/logging.h"

#include <tlhelp32.h>
#include <psapi.h>
#include <memory>

namespace aspia {

ProcessEnumerator::ProcessEnumerator()
{
    snapshot_.Reset(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot_.IsValid())
    {
        DLOG(LS_WARNING) << "CreateToolhelp32Snapshot() failed: " << GetLastSystemErrorString();
        return;
    }

    memset(&process_entry_, 0, sizeof(process_entry_));
    process_entry_.dwSize = sizeof(process_entry_);

    if (!Process32FirstW(snapshot_, &process_entry_))
    {
        DLOG(LS_WARNING) << "Process32FirstW() failed: " << GetLastSystemErrorString();
        snapshot_.Reset();
    }
    else
    {
        if (process_entry_.szExeFile[0] == '[')
        {
            Advance();
            return;
        }

        ScopedProcessPrivilege privilege(SE_DEBUG_NAME);

        current_process_.Reset(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                           FALSE,
                                           process_entry_.th32ProcessID));

        if (!current_process_.IsValid())
        {
            DLOG(LS_WARNING) << "OpenProcess() failed: " << GetLastSystemErrorString();
        }
    }
}

bool ProcessEnumerator::IsAtEnd() const
{
    return !snapshot_.IsValid();
}

void ProcessEnumerator::Advance()
{
    if (!Process32NextW(snapshot_, &process_entry_))
    {
        const DWORD error_code = GetLastError();
        if (error_code != ERROR_NO_MORE_FILES)
        {
            DLOG(LS_WARNING) << "Process32NextW() failed: " << GetLastSystemErrorString();
        }

        snapshot_.Reset();
    }
    else
    {
        ScopedProcessPrivilege privilege(SE_DEBUG_NAME);

        current_process_.Reset(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                           FALSE,
                                           process_entry_.th32ProcessID));
        if (!current_process_.IsValid())
        {
            DLOG(LS_WARNING) << "OpenProcess() failed: " << GetLastSystemErrorString();
        }
    }
}

std::string ProcessEnumerator::GetProcessName() const
{
    return UTF8fromUNICODE(process_entry_.szExeFile);
}

std::string ProcessEnumerator::GetFilePath() const
{
    if (!current_process_.IsValid())
        return std::string();

    WCHAR file_path[MAX_PATH];

    if (!GetModuleFileNameExW(current_process_.Get(), nullptr, file_path, _countof(file_path)))
    {
        DLOG(LS_WARNING) << "GetModuleFileNameExW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(file_path);
}

std::string ProcessEnumerator::GetFileDescription() const
{
    if (!current_process_.IsValid())
        return std::string();

    WCHAR file_path[MAX_PATH];

    if (!GetModuleFileNameExW(current_process_.Get(), nullptr, file_path, _countof(file_path)))
    {
        DLOG(LS_WARNING) << "GetModuleFileNameExW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(file_path, &handle);
    if (!size)
    {
        DLOG(LS_WARNING) << "GetFileVersionInfoSizeW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(size);

    if (!GetFileVersionInfoW(file_path, handle, size, buffer.get()))
    {
        DLOG(LS_WARNING) << "GetFileVersionInfoW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    struct LangAndCodepage
    {
        WORD language;
        WORD code_page;
    } *translate;

    UINT value_size;

    if (!VerQueryValueW(buffer.get(),
                        L"\\VarFileInfo\\Translation",
                        reinterpret_cast<LPVOID*>(&translate),
                        &value_size))
    {
        DLOG(LS_WARNING) << "VerQueryValueW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    std::wstring subblock = StringPrintf(L"\\StringFileInfo\\%04x%04x\\FileDescription",
                                         translate->language,
                                         translate->code_page);

    WCHAR* description = nullptr;

    if (!VerQueryValueW(buffer.get(),
                        subblock.c_str(),
                        reinterpret_cast<LPVOID*>(&description),
                        &value_size))
    {
        DLOG(LS_WARNING) << "VerQueryValueW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(description);
}

uint64_t ProcessEnumerator::GetUsedMemory() const
{
    if (!current_process_.IsValid())
        return 0;

    PROCESS_MEMORY_COUNTERS memory_counters;
    memset(&memory_counters, 0, sizeof(memory_counters));

    if (!GetProcessMemoryInfo(current_process_.Get(), &memory_counters, sizeof(memory_counters)))
    {
        DLOG(LS_WARNING) << "GetProcessMemoryInfo() failed: " << GetLastSystemErrorString();
        return 0;
    }

    return memory_counters.WorkingSetSize;
}

uint64_t ProcessEnumerator::GetUsedSwap() const
{
    if (!current_process_.IsValid())
        return 0;

    PROCESS_MEMORY_COUNTERS memory_counters;
    memset(&memory_counters, 0, sizeof(memory_counters));

    if (!GetProcessMemoryInfo(current_process_.Get(), &memory_counters, sizeof(memory_counters)))
    {
        DLOG(LS_WARNING) << "GetProcessMemoryInfo() failed: " << GetLastSystemErrorString();
        return 0;
    }

    return memory_counters.PagefileUsage;
}

} // namespace aspia
