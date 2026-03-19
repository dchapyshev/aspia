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

#include "host/host_utils.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <ShlObj.h>
#include <dxgi.h>
#include <d3d11.h>
#include <comdef.h>
#include <wrl/client.h>

#include "base/win/desktop.h"
#include "base/win/process_util.h"
#include "base/win/session_info.h"
#include "base/win/window_station.h"
#endif // defined(Q_OS_WINDOWS)

#include "build/version.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/peer/user_list.h"
#include "host/host_storage.h"
#include "host/system_settings.h"

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
QString configDir()
{
#if defined(Q_OS_WINDOWS)
    wchar_t buffer[MAX_PATH] = { 0 };

    HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer);
    if (FAILED(hr))
    {
        LOG(ERROR) << "SHGetFolderPathW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer);
#else
    return "/etc/aspia";
#endif
}

//--------------------------------------------------------------------------------------------------
QString hostFilePath()
{
    return configDir() + "/aspia/host.json";
}

//--------------------------------------------------------------------------------------------------
QString hostKeyFilePath()
{
    return configDir() + "/aspia/host_key.json";
}

//--------------------------------------------------------------------------------------------------
QString hostIpcFilePath()
{
    return configDir() + "/aspia/host_ipc.json";
}

//--------------------------------------------------------------------------------------------------
void doHostMigrate(const QJsonDocument& doc)
{
    SystemSettings settings;

    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate settings ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("ApplicationShutdownDisabled"))
    {
        bool value = root_object["ApplicationShutdownDisabled"].toString() == "true";
        LOG(INFO) << "ApplicationShutdownDisabled:" << value;
        settings.setApplicationShutdownDisabled(value);
    }

    if (root_object.contains("AutoConnConfirmInterval"))
    {
        qint64 value = root_object["AutoConnConfirmInterval"].toString().toLongLong();
        LOG(INFO) << "AutoConnConfirmInterval:" << value;
        settings.setAutoConfirmationInterval(std::chrono::milliseconds(value));
    }

    if (root_object.contains("AutoUpdateEnabled"))
    {
        bool value = root_object["AutoUpdateEnabled"].toString() == "true";
        LOG(INFO) << "AutoUpdateEnabled:" << value;
        settings.setAutoUpdateEnabled(value);
    }

    if (root_object.contains("ConnConfirm"))
    {
        bool value = root_object["ConnConfirm"].toString() == "true";
        LOG(INFO) << "ConnConfirm:" << value;
        settings.setConnectConfirmation(value);
    }

    if (root_object.contains("ConnConfirmNoUserAction"))
    {
        int value = root_object["ConnConfirmNoUserAction"].toString().toInt();
        LOG(INFO) << "ConnConfirmNoUserAction:" << value;
        settings.setNoUserAction(static_cast<SystemSettings::NoUserAction>(value));
    }

    if (root_object.contains("OneTimePassword"))
    {
        bool value = root_object["OneTimePassword"].toString() == "true";
        LOG(INFO) << "OneTimePassword:" << value;
        settings.setOneTimePassword(value);
    }

    if (root_object.contains("OneTimePasswordCharacters"))
    {
        quint32 value = root_object["OneTimePasswordCharacters"].toString().toUInt();
        LOG(INFO) << "OneTimePasswordCharacters:" << value;
        settings.setOneTimePasswordCharacters(value);
    }

    if (root_object.contains("OneTimePasswordExpire"))
    {
        qint64 value = root_object["OneTimePasswordExpire"].toString().toLongLong();
        LOG(INFO) << "OneTimePasswordExpire:" << value;
        settings.setOneTimePasswordExpire(std::chrono::milliseconds(value));
    }

    if (root_object.contains("OneTimePasswordLength"))
    {
        int value = root_object["OneTimePasswordLength"].toString().toInt();
        LOG(INFO) << "OneTimePasswordLength:" << value;
        settings.setOneTimePasswordLength(value);
    }

    if (root_object.contains("RouterAddress"))
    {
        QString value = root_object["RouterAddress"].toString();
        LOG(INFO) << "RouterAddress:" << value;
        settings.setRouterAddress(value);
    }

    if (root_object.contains("RouterEnabled"))
    {
        bool value = root_object["RouterEnabled"].toString() == "true";
        LOG(INFO) << "RouterEnabled:" << value;
        settings.setRouterEnabled(value);
    }

    if (root_object.contains("RouterPort"))
    {
        quint16 value = root_object["RouterPort"].toString().toUShort();
        LOG(INFO) << "RouterPort:" << value;
        settings.setRouterPort(value);
    }

    if (root_object.contains("RouterPublicKey"))
    {
        QString value = root_object["RouterPublicKey"].toString();
        LOG(INFO) << "RouterPublicKey:" << value;
        settings.setRouterPublicKey(QByteArray::fromHex(value.toLatin1()));
    }

    if (root_object.contains("TcpPort"))
    {
        quint16 value = root_object["TcpPort"].toString().toUShort();
        LOG(INFO) << "TcpPort:" << value;
        settings.setTcpPort(value);
    }

    if (root_object.contains("UpdateCheckFrequency"))
    {
        int value = root_object["UpdateCheckFrequency"].toString().toInt();
        LOG(INFO) << "UpdateCheckFrequency:" << value;
        settings.setUpdateCheckFrequency(value);
    }

    if (root_object.contains("UpdateServer"))
    {
        QString value = root_object["UpdateServer"].toString();
        LOG(INFO) << "UpdateServer:" << value;
        settings.setUpdateServer(value);
    }

    LOG(INFO) << "====== Migrate user list ======";

    std::unique_ptr<base::UserList> user_list = base::UserList::createEmpty();

    if (root_object.contains("SeedKey"))
    {
        QString value = root_object["SeedKey"].toString();
        LOG(INFO) << "SeedKey:" << value;
        user_list->setSeedKey(QByteArray::fromHex(value.toLatin1()));
    }

    QJsonObject users_object = root_object["Users"].toObject();
    if (!users_object.isEmpty())
    {
        int user_count = users_object["size"].toString().toInt();
        LOG(INFO) << "User count:" << user_count;

        for (int i = 1; i <= user_count; ++i)
        {
            QJsonObject user_object = users_object[QString::number(i)].toObject();
            base::User user;

            LOG(INFO) << "=== USER#" << i << "===";

            if (user_object.contains("Flags"))
            {
                int value = user_object["Flags"].toString().toInt();
                LOG(INFO) << "Flags:" << value;
                user.flags = value;
            }

            if (user_object.contains("Group"))
            {
                QString value = user_object["Group"].toString();
                LOG(INFO) << "Group:" << value;
                user.group = value;
            }

            if (user_object.contains("Name"))
            {
                QString value = user_object["Name"].toString();
                LOG(INFO) << "Name:" << value;
                user.name = value;
            }

            if (user_object.contains("Salt"))
            {
                QString value = user_object["Salt"].toString();
                LOG(INFO) << "Salt:" << value;
                user.salt = QByteArray::fromHex(value.toLatin1());
            }

            if (user_object.contains("Sessions"))
            {
                int value = user_object["Sessions"].toString().toInt();
                LOG(INFO) << "Sessions:" << value;
                user.sessions = value;
            }

            if (user_object.contains("Verifier"))
            {
                QString value = user_object["Verifier"].toString();
                LOG(INFO) << "Verifier:" << value;
                user.verifier = QByteArray::fromHex(value.toLatin1());
            }

            if (user.isValid())
            {
                LOG(INFO) << "Add user" << i << ":" << user.name;
                user_list->add(user);
            }
            else
            {
                LOG(ERROR) << "Invalid user" << i;
            }
        }
    }

    settings.setUserList(*user_list);
}

//--------------------------------------------------------------------------------------------------
void doHostKeyMigrate(const QJsonDocument& doc)
{
    HostStorage storage;
    QJsonObject root_object = doc.object();

    LOG(INFO) << "====== Migrate host key ======";

    if (root_object.isEmpty())
    {
        LOG(INFO) << "Root object is empty";
        return;
    }

    if (root_object.contains("console"))
    {
        QString value = root_object["console"].toString();
        LOG(INFO) << "console:" << value;
        storage.setHostKey(QByteArray::fromHex(value.toLatin1()));
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool HostUtils::isMigrationNeeded()
{
    QString host_file = hostFilePath();
    bool has_host_file = false;

    QString host_key_file = hostKeyFilePath();
    bool has_host_key_file = false;

    QString host_ipc_file = hostIpcFilePath();
    bool has_host_ipc_file = false;

    if (QFileInfo::exists(host_file))
    {
        LOG(INFO) << "Old configuration file exists:" << host_file;
        has_host_file = true;
    }
    else
    {
        LOG(INFO) << "Old configuration file does NOT exist:" << host_file;
    }

    if (QFileInfo::exists(host_key_file))
    {
        LOG(INFO) << "Old key file exists:" << host_key_file;
        has_host_key_file = true;
    }
    else
    {
        LOG(INFO) << "Old key file does NOT exist:" << host_key_file;
    }

    if (QFileInfo::exists(host_ipc_file))
    {
        LOG(INFO) << "Old IPC file exists:" << host_ipc_file;
        has_host_ipc_file = true;
    }
    else
    {
        LOG(INFO) << "Old IPC file does NOT exist:" << host_ipc_file;
    }

    return has_host_file || has_host_key_file || has_host_ipc_file;
}

//--------------------------------------------------------------------------------------------------
// static
void HostUtils::doMigrate()
{
    LOG(INFO) << "Start migration";

    QString host_file = hostFilePath();
    QString host_key_file = hostKeyFilePath();
    QString host_ipc_file = hostIpcFilePath();

    if (QFileInfo::exists(host_file))
    {
        LOG(INFO) << "Start migration for" << host_file;

        QFile file(host_file);
        if (!file.open(QFile::ReadOnly))
        {
            LOG(ERROR) << "Unable to open file" << host_file << ":" << file.errorString();
        }
        else
        {
            QByteArray buffer = file.readAll();
            file.close();

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(buffer, &parse_error);
            if (parse_error.error != QJsonParseError::NoError)
            {
                LOG(ERROR) << "JSON parse error at" << parse_error.offset << ":"
                           << parse_error.errorString();
            }
            else
            {
                doHostMigrate(doc);

                QString new_file_name = host_file + ".bak";

                if (QFile::rename(host_file, new_file_name))
                    LOG(INFO) << "File" << host_file << "is renamed to" << new_file_name;
                else
                    LOG(ERROR) << "Unable to rename file from" << host_file << "to" << new_file_name;
            }
        }
    }

    if (QFileInfo::exists(host_key_file))
    {
        LOG(INFO) << "Start migration for" << host_key_file;

        QFile file(host_key_file);
        if (!file.open(QFile::ReadOnly))
        {
            LOG(ERROR) << "Unable to open file" << host_key_file << ":" << file.errorString();
        }
        else
        {
            QByteArray buffer = file.readAll();
            file.close();

            QJsonParseError parse_error;
            QJsonDocument doc = QJsonDocument::fromJson(buffer, &parse_error);
            if (parse_error.error != QJsonParseError::NoError)
            {
                LOG(ERROR) << "JSON parse error at" << parse_error.offset << ":"
                           << parse_error.errorString();
            }
            else
            {
                doHostKeyMigrate(doc);

                QString new_file_name = host_key_file + ".bak";

                if (QFile::rename(host_key_file, new_file_name))
                    LOG(INFO) << "File" << host_key_file << "is renamed to" << new_file_name;
                else
                    LOG(ERROR) << "Unable to rename file from" << host_key_file << "to" << new_file_name;
            }
        }
    }

    if (QFileInfo::exists(host_ipc_file))
    {
        LOG(INFO) << "Start migration for" << host_key_file;

        QString new_file_name = host_ipc_file + ".bak";

        if (QFile::rename(host_ipc_file, new_file_name))
            LOG(INFO) << "File" << host_ipc_file << "is renamed to" << new_file_name;
        else
            LOG(ERROR) << "Unable to rename file from" << host_ipc_file << "to" << new_file_name;
    }

    LOG(INFO) << "Migration is DONE";
}

//--------------------------------------------------------------------------------------------------
// static
bool HostUtils::integrityCheck()
{
#if defined(Q_OS_WINDOWS)
    static const char* kFiles[] =
    {
        "aspia_host.exe",
        "aspia_desktop_agent.exe",
        "aspia_file_agent.exe",
        "aspia_host_service.exe"
    };
    static const size_t kMinFileSize = 50 * 1024; // 50 kB.

    QString current_dir = QCoreApplication::applicationDirPath();
    QString current_file = QCoreApplication::applicationFilePath();

    bool current_file_found = false;

    for (size_t i = 0; i < std::size(kFiles); ++i)
    {
        QString file_path(current_dir);

        file_path.append('/');
        file_path.append(kFiles[i]);

        if (file_path == current_file)
            current_file_found = true;

        QFileInfo file_info(file_path);

        if (!file_info.exists(file_path))
        {
            LOG(ERROR) << "File" << file_path << "does not exist";
            return false;
        }

        if (!file_info.isFile() || file_info.isShortcut() || file_info.isSymLink())
        {
            LOG(ERROR) << "File" << file_path << "is not a file";
            return false;
        }

        qint64 file_size = file_info.size();
        if (file_size < kMinFileSize)
        {
            LOG(ERROR) << "File" << file_path << "is not the correct size:" << file_size;
            return false;
        }
    }

    if (!current_file_found)
    {
        LOG(ERROR) << "Current executable file was not found in the list of components";
        return false;
    }
#endif // defined(Q_OS_WINDOWS)

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void HostUtils::printDebugInfo(quint32 features)
{
    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING
              << " (arch: " << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(INFO) << "Git branch:" << GIT_CURRENT_BRANCH;
    LOG(INFO) << "Git commit:" << GIT_COMMIT_HASH;
#endif
    LOG(INFO) << "Qt version:" << QT_VERSION_STR;
    LOG(INFO) << "Command line:" << QCoreApplication::arguments();
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

//--------------------------------------------------------------------------------------------------
// static
void HostUtils::uninstallApplication()
{
#if defined(Q_OS_WINDOWS)
    // MSI upgrade code for host package.
    constexpr wchar_t kUpgradeCode[] = L"{B460F717-1546-4FFC-9EDE-B21FD07E07CB}";

    QLibrary msiLibrary("msi");
    if (!msiLibrary.load())
    {
        LOG(ERROR) << "Unable to load msi library";
        return;
    }

    using MsiEnumRelatedProductsWFunc =
        UINT (WINAPI*)(LPCWSTR upgrade_code, DWORD reserved, DWORD product_index, LPWSTR product_buf);

    const auto msiEnumRelatedProducts =
        reinterpret_cast<MsiEnumRelatedProductsWFunc>(msiLibrary.resolve("MsiEnumRelatedProductsW"));
    if (!msiEnumRelatedProducts)
    {
        LOG(ERROR) << "Unable to find MsiEnumRelatedProductsW";
        return;
    }

    wchar_t productCode[MAX_PATH] = { 0 };
    const UINT rc = msiEnumRelatedProducts(kUpgradeCode, 0, 0, productCode);
    if (rc != ERROR_SUCCESS)
    {
        LOG(ERROR) << "MsiEnumRelatedProductsW failed:" << base::SystemError::toString(rc);
        return;
    }

    QProcess::startDetached("msiexec.exe", { "/x", QString::fromStdWString(productCode), "/qn" });
#else
    NOTIMPLEMENTED();
#endif
}

} // namespace host
