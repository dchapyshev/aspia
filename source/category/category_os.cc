//
// PROJECT:         Aspia
// FILE:            category/category_os.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/files/base_paths.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/datetime.h"
#include "base/registry.h"
#include "base/logging.h"
#include "category/category_os.h"
#include "category/category_os.pb.h"
#include "ui/resource.h"

#include <memory>

namespace aspia {

namespace {

std::string GetWindowVersion()
{
    std::experimental::filesystem::path kernel32_path;

    if (!GetBasePath(BasePathKey::DIR_SYSTEM, kernel32_path))
        return std::string();

    kernel32_path.append(L"kernel32.dll");

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(kernel32_path.c_str(), &handle);
    if (!size)
    {
        DPLOG(LS_WARNING) << "GetFileVersionInfoSizeW failed";
        return std::string();
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(size);

    if (!GetFileVersionInfoW(kernel32_path.c_str(), handle, size, buffer.get()))
    {
        DPLOG(LS_WARNING) << "GetFileVersionInfoW failed";
        return std::string();
    }

    struct LangAndCodepage
    {
        WORD language;
        WORD code_page;
    } *translate;

    UINT length = 0;

    if (!VerQueryValueW(buffer.get(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translate), &length))
    {
        DPLOG(LS_WARNING) << "VerQueryValueW failed";
        return std::string();
    }

    std::wstring subblock = StringPrintf(L"\\StringFileInfo\\%04x%04x\\ProductVersion",
                                         translate->language, translate->code_page);

    WCHAR* version = nullptr;

    if (!VerQueryValueW(buffer.get(), subblock.c_str(), reinterpret_cast<void**>(&version), &length))
    {
        DPLOG(LS_WARNING) << "VerQueryValueW failed";
        return std::string();
    }

    if (!version)
        return std::string();

    return UTF8fromUNICODE(version);
}

const char* ArchitectureToString(proto::OS::Architecture value)
{
    switch (value)
    {
        case proto::OS::ARCHITECTURE_X86:
            return "x86";

        case proto::OS::ARCHITECTURE_X86_64:
            return "x86_64";

        default:
            return "Unknown";
    }
}

std::string IntervalToString(uint64_t interval)
{
    uint64_t days = (interval / 86400);
    uint64_t hours = (interval % 86400) / 3600;
    uint64_t minutes = ((interval % 86400) % 3600) / 60;
    uint64_t seconds = ((interval % 86400) % 3600) % 60;

    return StringPrintf("%llu days %llu hours %llu minutes %llu seconds",
                        days, hours, minutes, seconds);
}

} // namespace

CategoryOS::CategoryOS()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryOS::Name() const
{
    return "Operating System";
}

Category::IconId CategoryOS::Icon() const
{
    return IDI_COMPUTER;
}

const char* CategoryOS::Guid() const
{
    return "{BD8F64BA-53DC-49CC-A943-5E2C45FFC4B9}";
}

void CategoryOS::Parse(Table& table, const std::string& data)
{
    proto::OS message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    {
        Group os_group = table.AddGroup("Operating System");

        if (!message.os_name().empty())
            os_group.AddParam("Name", Value::String(message.os_name()));

        if (!message.os_version().empty())
            os_group.AddParam("Version", Value::String(message.os_version()));

        os_group.AddParam("Architecture", Value::String(ArchitectureToString(message.os_architecture())));

        if (message.install_date())
            os_group.AddParam("Install Date", Value::String(TimeToString(message.install_date())));

        if (!message.system_root().empty())
            os_group.AddParam("System Root", Value::String(message.system_root()));
    }

    {
        Group computer_group = table.AddGroup("Computer");

        if (!message.computer_name().empty())
            computer_group.AddParam("Computer Name", Value::String(message.computer_name()));

        computer_group.AddParam("CPU Architecture", Value::String(
            ArchitectureToString(message.cpu_architecture())));

        computer_group.AddParam("Local Time", Value::String(TimeToString(message.local_time())));
        computer_group.AddParam("Uptime", Value::String(IntervalToString(message.uptime())));
    }
}

std::string CategoryOS::Serialize()
{
    proto::OS message;

#if (ARCH_CPU_X86 == 1)
    BOOL is_wow64_process = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64_process);
#endif

    RegistryKey key;

    REGSAM access = KEY_READ;

#if (ARCH_CPU_X86 == 1)
    if (is_wow64_process)
        access |= KEY_WOW64_64KEY;
#endif

    LONG status = key.Open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access);
    if (status == ERROR_SUCCESS)
    {
        std::wstring value;

        status = key.ReadValue(L"ProductName", &value);
        if (status == ERROR_SUCCESS)
        {
            message.set_os_name(UTF8fromUNICODE(value));
        }

        DWORD install_date;

        status = key.ReadValueDW(L"InstallDate", &install_date);
        if (status == ERROR_SUCCESS)
        {
            message.set_install_date(install_date);
        }
    }

    message.set_os_version(GetWindowVersion());

    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);

    switch (system_info.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
        {
            message.set_cpu_architecture(proto::OS::ARCHITECTURE_X86_64);

#if (ARCH_CPU_X86 == 1)
            message.set_os_architecture(
                is_wow64_process ? proto::OS::ARCHITECTURE_X86_64 : proto::OS::ARCHITECTURE_X86);
#elif (ARCH_CPU_X86_64 == 1)
            message.set_os_architecture(proto::OS::ARCHITECTURE_X86_64);
#else
#error Unknown architecture
#endif
        }
        break;

        case PROCESSOR_ARCHITECTURE_INTEL:
            message.set_cpu_architecture(proto::OS::ARCHITECTURE_X86);
            message.set_os_architecture(proto::OS::ARCHITECTURE_X86);
            break;

        default:
            break;
    }

    std::experimental::filesystem::path system_root;
    if (GetBasePath(BasePathKey::DIR_WINDOWS, system_root))
    {
        message.set_system_root(system_root.u8string());
    }

    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (QueryPerformanceCounter(&counter) && QueryPerformanceFrequency(&frequency))
    {
        message.set_uptime(counter.QuadPart / frequency.QuadPart);
    }

    WCHAR computer_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD computer_name_size = ARRAYSIZE(computer_name);

    if (GetComputerNameW(computer_name, &computer_name_size))
    {
        message.set_computer_name(UTF8fromUNICODE(computer_name));
    }

    message.set_local_time(std::time(nullptr));

    return message.SerializeAsString();
}

} // namespace aspia
