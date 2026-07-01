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

#include "base/sys_info.h"

#include <qt_windows.h>

#include <LM.h>
#include <devguid.h>
#include <winspool.h>

#include <memory>

#include "base/logging.h"
#include "base/session_id.h"
#include "base/system_error.h"
#include "base/win/registry.h"
#include "base/win/scoped_device_info.h"
#include "base/win/scoped_object.h"
#include "base/win/scoped_wts_memory.h"
#include "base/win/session_info.h"
#include "base/win/windows_version.h"

namespace {

const char kClassRootPath[] = "SYSTEM\\CurrentControlSet\\Control\\Class\\";
const char kDriverVersionKey[] = "DriverVersion";
const char kDriverDateKey[] = "DriverDate";
const char kProviderNameKey[] = "ProviderName";

//--------------------------------------------------------------------------------------------------
SysInfo::Service::Status serviceStatus(DWORD state)
{
    switch (state)
    {
        case SERVICE_CONTINUE_PENDING: return SysInfo::Service::Status::CONTINUE_PENDING;
        case SERVICE_PAUSE_PENDING:    return SysInfo::Service::Status::PAUSE_PENDING;
        case SERVICE_PAUSED:           return SysInfo::Service::Status::PAUSED;
        case SERVICE_RUNNING:          return SysInfo::Service::Status::RUNNING;
        case SERVICE_START_PENDING:    return SysInfo::Service::Status::START_PENDING;
        case SERVICE_STOP_PENDING:     return SysInfo::Service::Status::STOP_PENDING;
        case SERVICE_STOPPED:          return SysInfo::Service::Status::STOPPED;
        default:                       return SysInfo::Service::Status::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
SysInfo::Service::StartupType serviceStartupType(DWORD start_type)
{
    switch (start_type)
    {
        case SERVICE_AUTO_START:   return SysInfo::Service::StartupType::AUTO_START;
        case SERVICE_DEMAND_START: return SysInfo::Service::StartupType::DEMAND_START;
        case SERVICE_DISABLED:     return SysInfo::Service::StartupType::DISABLED;
        case SERVICE_BOOT_START:   return SysInfo::Service::StartupType::BOOT_START;
        case SERVICE_SYSTEM_START: return SysInfo::Service::StartupType::SYSTEM_START;
        default:                   return SysInfo::Service::StartupType::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
QString serviceDescription(SC_HANDLE service_handle)
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfig2W(service_handle, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        return QString();
    }

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);

    if (!QueryServiceConfig2W(service_handle, SERVICE_CONFIG_DESCRIPTION, buffer.get(), bytes_needed,
        &bytes_needed))
    {
        return QString();
    }

    auto* description = reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.get());
    if (!description->lpDescription)
        return QString();

    return QString::fromWCharArray(description->lpDescription);
}

//--------------------------------------------------------------------------------------------------
QList<SysInfo::Service> enumerateServices(DWORD service_type)
{
    QList<SysInfo::Service> result;

    ScopedScHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return result;
    }

    DWORD bytes_needed = 0;
    DWORD services_count = 0;
    DWORD resume_handle = 0;

    if (EnumServicesStatusExW(manager, SC_ENUM_PROCESS_INFO, service_type, SERVICE_STATE_ALL,
        nullptr, 0, &bytes_needed, &services_count, &resume_handle, nullptr) ||
        GetLastError() != ERROR_MORE_DATA)
    {
        PLOG(ERROR) << "Unexpected return value";
        return result;
    }

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);

    if (!EnumServicesStatusExW(manager, SC_ENUM_PROCESS_INFO, service_type, SERVICE_STATE_ALL,
        buffer.get(), bytes_needed, &bytes_needed, &services_count, &resume_handle, nullptr))
    {
        PLOG(ERROR) << "EnumServicesStatusExW failed";
        return result;
    }

    auto* status_array = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(buffer.get());
    for (DWORD i = 0; i < services_count; ++i)
    {
        const ENUM_SERVICE_STATUS_PROCESS& src = status_array[i];

        SysInfo::Service service;
        if (src.lpServiceName)
            service.name = QString::fromWCharArray(src.lpServiceName);
        if (src.lpDisplayName)
            service.display_name = QString::fromWCharArray(src.lpDisplayName);
        service.status = serviceStatus(src.ServiceStatusProcess.dwCurrentState);

        ScopedScHandle service_handle(OpenServiceW(manager, src.lpServiceName,
            SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS));
        if (service_handle.isValid())
        {
            service.description = serviceDescription(service_handle);

            DWORD config_bytes = 0;
            if (!QueryServiceConfigW(service_handle, nullptr, 0, &config_bytes) &&
                GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                std::unique_ptr<quint8[]> config_buffer = std::make_unique<quint8[]>(config_bytes);
                auto* config = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(config_buffer.get());

                if (QueryServiceConfigW(service_handle, config, config_bytes, &config_bytes))
                {
                    service.startup_type = serviceStartupType(config->dwStartType);
                    if (config->lpBinaryPathName)
                        service.binary_path = QString::fromWCharArray(config->lpBinaryPathName);
                    if (config->lpServiceStartName)
                        service.start_name = QString::fromWCharArray(config->lpServiceStartName);
                }
            }
        }

        result.append(std::move(service));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool isWow64Process()
{
    BOOL is_wow64_process = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64_process);
    return !!is_wow64_process;
}

//--------------------------------------------------------------------------------------------------
int processorCount(LOGICAL_PROCESSOR_RELATIONSHIP relationship)
{
    DWORD returned_length = 0;
    int count = 0;

    if (GetLogicalProcessorInformation(nullptr, &returned_length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(ERROR) << "Unexpected return value";
        return 0;
    }

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(returned_length);

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.get());

    if (!GetLogicalProcessorInformation(info, &returned_length))
    {
        PLOG(ERROR) << "GetLogicalProcessorInformation failed";
        return 0;
    }

    returned_length /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    for (DWORD i = 0; i < returned_length; ++i)
    {
        if (info[i].Relationship == relationship)
            ++count;
    }

    return count;
}

//--------------------------------------------------------------------------------------------------
QString digitalProductIdToString(quint8* product_id, size_t product_id_size)
{
    constexpr char kKeyMap[] = "BCDFGHJKMPQRTVWXY2346789";
    constexpr int kKeyMapSize = 24;
    constexpr int kStartIndex = 52;
    constexpr int kDecodeLength = 25;
    constexpr int kDecodeStringLength = 15;
    constexpr int kGroupLength = 5;

    if (product_id_size < kStartIndex + kDecodeLength)
        return QString();

    // The keys starting with Windows 8 / Office 2013 can contain the symbol N.
    int containsN = (product_id[kStartIndex + 14] >> 3) & 1;
    product_id[kStartIndex + 14] =
        static_cast<quint8>((product_id[kStartIndex + 14] & 0xF7) | ((containsN & 2) << 2));

    std::u16string key;

    for (int i = kDecodeLength - 1; i >= 0; --i)
    {
        int key_map_index = 0;

        for (int j = kDecodeStringLength - 1; j >= 0; --j)
        {
            key_map_index = (key_map_index << 8) | product_id[kStartIndex + j];
            product_id[kStartIndex + j] = static_cast<quint8>(key_map_index / kKeyMapSize);
            key_map_index %= kKeyMapSize;
        }

        key.insert(key.begin(), kKeyMap[key_map_index]);
    }

    if (containsN)
    {
        // Skip the first character.
        key.erase(key.begin());

        // Insert the symbol N after the first group.
        key.insert(kGroupLength, 1, u'N');
    }

    for (size_t i = kGroupLength; i < key.length(); i += kGroupLength + 1)
    {
        // Insert group separators.
        key.insert(i, 1, u'-');
    }

    return QString::fromStdU16String(key);
}

//--------------------------------------------------------------------------------------------------
// Reads a string device-registry property (SPDRP_*), or an empty string on failure.
QString deviceProperty(HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data, DWORD property)
{
    wchar_t buffer[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info, device_info_data, property, nullptr,
        reinterpret_cast<PBYTE>(buffer), ARRAYSIZE(buffer), nullptr))
    {
        return QString();
    }

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
QString deviceInstanceId(HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data)
{
    wchar_t device_id[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceInstanceIdW(device_info, device_info_data, device_id, ARRAYSIZE(device_id),
        nullptr))
    {
        PLOG(ERROR) << "SetupDiGetDeviceInstanceIdW failed";
        return QString();
    }

    return QString::fromWCharArray(device_id);
}

//--------------------------------------------------------------------------------------------------
QString driverKeyPath(HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data)
{
    QString driver = deviceProperty(device_info, device_info_data, SPDRP_DRIVER);
    if (driver.isEmpty())
        return QString();

    return kClassRootPath + driver;
}

//--------------------------------------------------------------------------------------------------
QString driverRegistryString(HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data,
                             const QString& value_name)
{
    QString key_path = driverKeyPath(device_info, device_info_data);
    if (key_path.isEmpty())
        return QString();

    RegKey key(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (!key.isValid())
        return QString();

    wchar_t value[MAX_PATH] = { 0 };
    DWORD value_size = ARRAYSIZE(value);

    if (key.readValue(value_name, value, &value_size, nullptr) != ERROR_SUCCESS)
        return QString();

    return QString::fromWCharArray(value);
}

//--------------------------------------------------------------------------------------------------
DWORD driverRegistryDW(HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data,
                       const QString& value_name)
{
    QString key_path = driverKeyPath(device_info, device_info_data);
    if (key_path.isEmpty())
        return 0;

    RegKey key(HKEY_LOCAL_MACHINE, key_path, KEY_READ);
    if (!key.isValid())
        return 0;

    DWORD value = 0;
    if (key.readValueDW(value_name, &value) != ERROR_SUCCESS)
        return 0;

    return value;
}

} // namespace

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    OSInfo* os_info = OSInfo::instance();

    if (os_info->version() >= VERSION_WIN11)
    {
        // Key ProductName in the Windows 11 registry says it's Windows 10.
        // We can't rely on this value.
        switch (os_info->versionType())
        {
            case SUITE_HOME:
                return "Windows 11 Home";
            case SUITE_PROFESSIONAL:
                return "Windows 11 Pro";
            case SUITE_SERVER:
                return "Windows 11 Server";
            case SUITE_ENTERPRISE:
                return "Windows 11 Enterprise";
            case SUITE_EDUCATION:
                return "Windows 11 Education";
            case SUITE_EDUCATION_PRO:
                return "Windows 11 Education Pro";
            default:
                return "Windows 11";
        }
    }

    RegKey key;

    REGSAM access = KEY_READ;

#if defined(Q_PROCESSOR_X86_32)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif

    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to open registry key:" << SystemError::toString(status);
        return QString();
    }

    QString value;

    status = key.readValue("ProductName", &value);
    if (status != ERROR_SUCCESS)
    {
        LOG(ERROR) << "Unable to read registry key:" << SystemError::toString(status);
        return QString();
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    return OSInfo::instance()->kernel32BaseVersion().toString();
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
    switch (OSInfo::instance()->architecture())
    {
        case OSInfo::X64_ARCHITECTURE:
            return "AMD64";

        case OSInfo::X86_ARCHITECTURE:
            return "X86";

        case OSInfo::IA64_ARCHITECTURE:
            return "IA64";

        case OSInfo::ARM_ARCHITECTURE:
            return "ARM";

        default:
            return QString();
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemDir()
{
    return qEnvironmentVariable("WINDIR");
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemKey()
{
    RegKey key;

    REGSAM access = KEY_READ;

#if defined(Q_PROCESSOR_X86_32)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif

    // Read MS Windows Key.
    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access | KEY_READ);
    if (status != ERROR_SUCCESS)
        return QString();

    DWORD product_id_size = 0;

    status = key.readValue("DigitalProductId", nullptr, &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue("DPID", nullptr, &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return QString();
    }

    std::unique_ptr<quint8[]> product_id = std::make_unique<quint8[]>(product_id_size);

    status = key.readValue("DigitalProductId", product_id.get(), &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue("DPID", product_id.get(), &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return QString();
    }

    return digitalProductIdToString(product_id.get(), product_id_size);
}

//--------------------------------------------------------------------------------------------------
// static
qint64 SysInfo::operatingSystemInstallDate()
{
    RegKey key;

    REGSAM access = KEY_READ;

#if defined(Q_PROCESSOR_X86_32)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif

    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access);
    if (status == ERROR_SUCCESS)
    {
        DWORD install_date;

        status = key.readValueDW("InstallDate", &install_date);
        if (status == ERROR_SUCCESS)
            return static_cast<qint64>(install_date);
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
quint64 SysInfo::uptime()
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (!QueryPerformanceCounter(&counter))
    {
        PLOG(ERROR) << "QueryPerformanceCounter failed";
        return 0;
    }

    if (!QueryPerformanceFrequency(&frequency))
    {
        PLOG(ERROR) << "QueryPerformanceFrequency failed";
        return 0;
    }

    return static_cast<quint64>(counter.QuadPart / frequency.QuadPart);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buffer_size = ARRAYSIZE(buffer);

    if (!GetComputerNameW(buffer, &buffer_size))
    {
        PLOG(ERROR) << "GetComputerNameW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerDomain()
{
    DWORD buffer_size = 0;

    if (GetComputerNameExW(ComputerNameDnsDomain, nullptr, &buffer_size) ||
        GetLastError() != ERROR_MORE_DATA)
    {
        LOG(ERROR) << "Unexpected return value";
        return QString();
    }

    std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(buffer_size);

    if (!GetComputerNameExW(ComputerNameDnsDomain, buffer.get(), &buffer_size))
    {
        PLOG(ERROR) << "GetComputerNameExW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer.get());
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    NETSETUP_JOIN_STATUS buffer_type = NetSetupWorkgroupName;
    wchar_t* buffer = nullptr;

    DWORD ret = NetGetJoinInformation(nullptr, &buffer, &buffer_type);
    if (ret != NERR_Success)
    {
        LOG(ERROR) << "NetGetJoinInformation failed:" << ret;
        return QString();
    }

    if (!buffer)
        return QString();

    QString result = QString::fromWCharArray(buffer);
    NetApiBufferFree(buffer);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorPackages()
{
    return processorCount(RelationProcessorPackage);
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    return processorCount(RelationProcessorCore);
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorThreads()
{
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray SysInfo::smbiosDump()
{
    UINT buffer_size = GetSystemFirmwareTable('RSMB', 'PCAF', nullptr, 0);
    if (!buffer_size)
    {
        PLOG(ERROR) << "GetSystemFirmwareTable failed";
        return QByteArray();
    }

    QByteArray buffer;
    buffer.resize(static_cast<qsizetype>(buffer_size));

    if (!GetSystemFirmwareTable('RSMB', 'PCAF', buffer.data(), buffer_size))
    {
        PLOG(ERROR) << "GetSystemFirmwareTable failed";
        return QByteArray();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::User> SysInfo::users()
{
    QList<User> result;

    USER_INFO_3* user_info = nullptr;
    DWORD entries_read = 0;
    DWORD total_entries = 0;

    DWORD error_code = NetUserEnum(nullptr, 3, FILTER_NORMAL_ACCOUNT,
        reinterpret_cast<LPBYTE*>(&user_info), MAX_PREFERRED_LENGTH, &entries_read, &total_entries,
        nullptr);
    if (error_code != NERR_Success)
    {
        LOG(ERROR) << "NetUserEnum failed:" << SystemError(error_code).toString();
        return result;
    }

    for (DWORD i = 0; i < total_entries; ++i)
    {
        const USER_INFO_3& src = user_info[i];
        User user;

        if (src.usri3_name)
            user.name = QString::fromWCharArray(src.usri3_name);
        if (src.usri3_full_name)
            user.full_name = QString::fromWCharArray(src.usri3_full_name);
        if (src.usri3_home_dir)
            user.home_dir = QString::fromWCharArray(src.usri3_home_dir);

        user.disabled = (src.usri3_flags & UF_ACCOUNTDISABLE) != 0;
        user.password_expired = (src.usri3_flags & UF_PASSWORD_EXPIRED) != 0;
        user.dont_expire_password = (src.usri3_flags & UF_DONT_EXPIRE_PASSWD) != 0;
        user.last_logon_time = src.usri3_last_logon;

        if (src.usri3_name)
        {
            LPLOCALGROUP_USERS_INFO_0 member_info = nullptr;
            DWORD member_entries_read = 0;
            DWORD member_total_entries = 0;

            DWORD member_error = NetUserGetLocalGroups(nullptr, src.usri3_name, 0, LG_INCLUDE_INDIRECT,
                reinterpret_cast<LPBYTE*>(&member_info), MAX_PREFERRED_LENGTH, &member_entries_read,
                &member_total_entries);
            if (member_error == NERR_Success)
            {
                for (DWORD j = 0; j < member_total_entries; ++j)
                {
                    if (!member_info[j].lgrui0_name)
                        continue;

                    UserGroup group;
                    group.name = QString::fromWCharArray(member_info[j].lgrui0_name);

                    user.groups.append(std::move(group));
                }

                NetApiBufferFree(member_info);
            }
            else
            {
                LOG(ERROR) << "NetUserGetLocalGroups failed:" << SystemError(member_error).toString();
            }
        }

        result.append(std::move(user));
    }

    NetApiBufferFree(user_info);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::UserGroup> SysInfo::userGroups()
{
    QList<UserGroup> result;

    LOCALGROUP_INFO_1* group_info = nullptr;
    DWORD entries_read = 0;
    DWORD total_entries = 0;

    DWORD error_code = NetLocalGroupEnum(nullptr, 1, reinterpret_cast<LPBYTE*>(&group_info),
        MAX_PREFERRED_LENGTH, &entries_read, &total_entries, nullptr);
    if (error_code != NERR_Success)
    {
        LOG(ERROR) << "NetLocalGroupEnum failed:" << SystemError(error_code).toString();
        return result;
    }

    for (DWORD i = 0; i < total_entries; ++i)
    {
        UserGroup group;

        if (group_info[i].lgrpi1_name)
            group.name = QString::fromWCharArray(group_info[i].lgrpi1_name);

        result.append(std::move(group));
    }

    NetApiBufferFree(group_info);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::services()
{
    return enumerateServices(SERVICE_WIN32);
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::drivers()
{
    return enumerateServices(SERVICE_DRIVER);
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Session> SysInfo::sessions()
{
    QList<Session> result;

    DWORD level = 1;
    ScopedWtsMemory<WTS_SESSION_INFO_1W> info;
    DWORD count = 0;

    if (!WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level, 0, info.receive(), &count))
    {
        PLOG(ERROR) << "WTSEnumerateSessionsExW failed";
        return result;
    }

    for (DWORD i = 0; i < count; ++i)
    {
        SessionId session_id = info[i]->SessionId;
        if (session_id == kServiceSessionId) // Don't add the system session.
            continue;

        SessionInfo session_info(session_id);
        if (!session_info.isValid())
            continue;

        Session session;
        session.id           = session_id;
        session.user_name    = session_info.userName();
        session.domain_name  = session_info.domain();
        session.session_name = session_info.winStationName();
        session.client_name  = session_info.clientName();
        session.locked       = session_info.isUserLocked();

        switch (session_info.connectState())
        {
            case SessionInfo::ConnectState::ACTIVE:
                session.connect_state = Session::ConnectState::ACTIVE;
                break;

            case SessionInfo::ConnectState::CONNECTED:
                session.connect_state = Session::ConnectState::CONNECTED;
                break;

            case SessionInfo::ConnectState::CONNECT_QUERY:
                session.connect_state = Session::ConnectState::CONNECT_QUERY;
                break;

            case SessionInfo::ConnectState::SHADOW:
                session.connect_state = Session::ConnectState::SHADOW;
                break;

            case SessionInfo::ConnectState::DISCONNECTED:
                session.connect_state = Session::ConnectState::DISCONNECTED;
                break;

            case SessionInfo::ConnectState::IDLE:
                session.connect_state = Session::ConnectState::IDLE;
                break;

            case SessionInfo::ConnectState::LISTEN:
                session.connect_state = Session::ConnectState::LISTEN;
                break;

            case SessionInfo::ConnectState::RESET:
                session.connect_state = Session::ConnectState::RESET;
                break;

            case SessionInfo::ConnectState::DOWN:
                session.connect_state = Session::ConnectState::DOWN;
                break;

            case SessionInfo::ConnectState::INIT:
                session.connect_state = Session::ConnectState::INIT;
                break;

            default:
                session.connect_state = Session::ConnectState::UNKNOWN;
                break;
        }

        result.append(std::move(session));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Monitor> SysInfo::monitors()
{
    QList<Monitor> result;

    ScopedDeviceInfo device_info(SetupDiGetClassDevsW(
        &GUID_DEVCLASS_MONITOR, nullptr, nullptr, DIGCF_PROFILE | DIGCF_PRESENT));
    if (!device_info.isValid())
    {
        PLOG(ERROR) << "SetupDiGetClassDevsW failed";
        return result;
    }

    HDEVINFO info = device_info.get();

    SP_DEVINFO_DATA data;
    memset(&data, 0, sizeof(data));
    data.cbSize = sizeof(data);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &data); ++index)
    {
        QString key_path = QString("SYSTEM\\CurrentControlSet\\Enum\\%1\\Device Parameters")
                               .arg(deviceInstanceId(info, &data));

        RegKey key;
        if (key.open(HKEY_LOCAL_MACHINE, key_path, KEY_READ) != ERROR_SUCCESS)
            continue;

        QByteArray edid;
        if (key.readValueBIN("EDID", &edid) != ERROR_SUCCESS)
            continue;

        Monitor monitor;
        monitor.system_name = deviceProperty(info, &data, SPDRP_FRIENDLYNAME);
        if (monitor.system_name.isEmpty())
            monitor.system_name = deviceProperty(info, &data, SPDRP_DEVICEDESC);
        monitor.edid = std::move(edid);

        result.append(std::move(monitor));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::VideoAdapter> SysInfo::videoAdapters()
{
    QList<VideoAdapter> result;

    ScopedDeviceInfo device_info(SetupDiGetClassDevsW(
        &GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DIGCF_PROFILE | DIGCF_PRESENT));
    if (!device_info.isValid())
    {
        PLOG(ERROR) << "SetupDiGetClassDevsW failed";
        return result;
    }

    HDEVINFO info = device_info.get();

    SP_DEVINFO_DATA data;
    memset(&data, 0, sizeof(data));
    data.cbSize = sizeof(data);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &data); ++index)
    {
        VideoAdapter adapter;
        adapter.description = deviceProperty(info, &data, SPDRP_DEVICEDESC);
        adapter.adapter_string = driverRegistryString(info, &data, "HardwareInformation.AdapterString");
        adapter.bios_string = driverRegistryString(info, &data, "HardwareInformation.BiosString");
        adapter.chip_type = driverRegistryString(info, &data, "HardwareInformation.ChipType");
        adapter.dac_type = driverRegistryString(info, &data, "HardwareInformation.DacType");
        adapter.driver_date = driverRegistryString(info, &data, kDriverDateKey);
        adapter.driver_version = driverRegistryString(info, &data, kDriverVersionKey);
        adapter.driver_provider = driverRegistryString(info, &data, kProviderNameKey);
        adapter.memory_size = driverRegistryDW(info, &data, "HardwareInformation.MemorySize");

        result.append(std::move(adapter));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Device> SysInfo::devices()
{
    QList<Device> result;

    ScopedDeviceInfo device_info(SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE));
    if (!device_info.isValid())
    {
        PLOG(ERROR) << "SetupDiGetClassDevsW failed";
        return result;
    }

    HDEVINFO info = device_info.get();

    SP_DEVINFO_DATA data;
    memset(&data, 0, sizeof(data));
    data.cbSize = sizeof(data);

    for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &data); ++index)
    {
        Device device;
        device.friendly_name = deviceProperty(info, &data, SPDRP_FRIENDLYNAME);
        device.description = deviceProperty(info, &data, SPDRP_DEVICEDESC);
        device.driver_vendor = driverRegistryString(info, &data, kProviderNameKey);
        device.device_id = deviceInstanceId(info, &data);

        result.append(std::move(device));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Printer> SysInfo::printers()
{
    QList<Printer> result;

    const DWORD flags = PRINTER_ENUM_FAVORITE | PRINTER_ENUM_LOCAL | PRINTER_ENUM_NETWORK;
    DWORD bytes_needed = 0;
    DWORD count = 0;

    if (EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytes_needed, &count) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        PLOG(ERROR) << "Unexpected return value";
        return result;
    }

    QByteArray info_buffer;
    info_buffer.resize(bytes_needed);

    if (!EnumPrintersW(flags, nullptr, 2, reinterpret_cast<LPBYTE>(info_buffer.data()), bytes_needed,
                       &bytes_needed, &count))
    {
        PLOG(ERROR) << "EnumPrintersW failed";
        return result;
    }

    std::wstring default_printer;
    wchar_t default_printer_name[256] = { 0 };
    DWORD characters_count = ARRAYSIZE(default_printer_name);
    if (GetDefaultPrinterW(default_printer_name, &characters_count))
        default_printer = default_printer_name;

    const PRINTER_INFO_2W* info = reinterpret_cast<const PRINTER_INFO_2W*>(info_buffer.constData());
    for (DWORD i = 0; i < count; ++i)
    {
        const PRINTER_INFO_2W& item = info[i];

        Printer printer;
        if (item.pPrinterName)
            printer.name = QString::fromWCharArray(item.pPrinterName);
        if (item.pShareName)
            printer.share_name = QString::fromWCharArray(item.pShareName);
        if (item.pPortName)
            printer.port_name = QString::fromWCharArray(item.pPortName);
        if (item.pDriverName)
            printer.driver_name = QString::fromWCharArray(item.pDriverName);
        printer.is_default = item.pPrinterName && default_printer == item.pPrinterName;
        printer.is_shared = (item.Attributes & PRINTER_ATTRIBUTE_SHARED) != 0;
        printer.jobs_count = static_cast<int>(item.cJobs);

        result.append(std::move(printer));
    }

    return result;
}
