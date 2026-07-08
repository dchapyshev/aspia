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

#include "base/service_controller_win.h"

#include <QDir>

#include <aclapi.h>

#include "base/logging.h"
#include "base/win/scoped_local.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool grantFileAccess(const QString& account, const QString& path)
{
    // Resolve the account name (for example "NT SERVICE\\aspia-router") to a SID.
    BYTE sid[SECURITY_MAX_SID_SIZE];
    DWORD sid_size = sizeof(sid);
    wchar_t domain[256];
    DWORD domain_size = _countof(domain);
    SID_NAME_USE name_use;

    if (!LookupAccountNameW(nullptr, qUtf16Printable(account), sid, &sid_size, domain,
                            &domain_size, &name_use))
    {
        PLOG(ERROR) << "LookupAccountNameW failed for" << account;
        return false;
    }

    std::wstring native_path = QDir::toNativeSeparators(path).toStdWString();

    // Read the current DACL so the new ACE is merged into it instead of replacing existing rights.
    PACL old_dacl = nullptr;
    ScopedLocal<PSECURITY_DESCRIPTOR> sd;
    DWORD result = GetNamedSecurityInfoW(native_path.c_str(), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, &old_dacl, nullptr, sd.recieve());
    if (result != ERROR_SUCCESS)
    {
        SetLastError(result);
        PLOG(ERROR) << "GetNamedSecurityInfoW failed for" << path;
        return false;
    }

    EXPLICIT_ACCESSW ea;
    memset(&ea, 0, sizeof(ea));

    // "Modify": read, write and delete, but not changing permissions or taking ownership.
    ea.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE;
    ea.grfAccessMode        = GRANT_ACCESS;
    ea.grfInheritance       = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_UNKNOWN;
    ea.Trustee.ptstrName    = reinterpret_cast<LPWSTR>(sid);

    ScopedLocal<PACL> new_dacl;
    result = SetEntriesInAclW(1, &ea, old_dacl, new_dacl.recieve());
    if (result != ERROR_SUCCESS)
    {
        SetLastError(result);
        PLOG(ERROR) << "SetEntriesInAclW failed";
        return false;
    }

    result = SetNamedSecurityInfoW(native_path.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                   nullptr, nullptr, new_dacl.get(), nullptr);
    if (result != ERROR_SUCCESS)
    {
        SetLastError(result);
        PLOG(ERROR) << "SetNamedSecurityInfoW failed for" << path;
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
ServiceControllerWin::ServiceControllerWin(SC_HANDLE sc_manager, SC_HANDLE service)
    : sc_manager_(sc_manager),
      service_(service)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
ServiceControllerWin::~ServiceControllerWin() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerWin::open(const QString& name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return nullptr;
    }

    ScopedScHandle service(OpenServiceW(sc_manager, qUtf16Printable(name), SERVICE_ALL_ACCESS));
    if (!service.isValid())
    {
        PLOG(ERROR) << "OpenServiceW failed";
        return nullptr;
    }

    return std::make_unique<ServiceControllerWin>(sc_manager.release(), service.release());
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<ServiceController> ServiceControllerWin::install(
    const QString& name, const QString& display_name, const QString& file_path,
    const QStringList& arguments)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return nullptr;
    }

    // The SCM runs the whole lpBinaryPathName as the command line, so quote the executable (its path
    // may contain spaces) and append the service-mode arguments.
    QString binary_path = '"' + QDir::toNativeSeparators(file_path) + '"';
    if (!arguments.isEmpty())
        binary_path += ' ' + arguments.join(' ');

    ScopedScHandle service(CreateServiceW(sc_manager, qUtf16Printable(name),
        qUtf16Printable(display_name), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, qUtf16Printable(binary_path),
        nullptr, nullptr, nullptr, nullptr, nullptr));
    if (!service.isValid())
    {
        PLOG(ERROR) << "CreateServiceW failed";
        return nullptr;
    }

    SC_ACTION action;
    action.Type = SC_ACTION_RESTART;
    action.Delay = 60000; // 60 seconds

    SERVICE_FAILURE_ACTIONS actions;
    actions.dwResetPeriod = 0;
    actions.lpRebootMsg   = nullptr;
    actions.lpCommand     = nullptr;
    actions.cActions      = 1;
    actions.lpsaActions   = &action;

    if (!ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &actions))
    {
        PLOG(ERROR) << "ChangeServiceConfig2W failed";
        return nullptr;
    }

    return std::make_unique<ServiceControllerWin>(sc_manager.release(), service.release());
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerWin::remove(const QString& name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return false;
    }

    ScopedScHandle service(OpenServiceW(sc_manager, qUtf16Printable(name), SERVICE_ALL_ACCESS));
    if (!service.isValid())
    {
        PLOG(ERROR) << "OpenServiceW failed";
        return false;
    }

    if (!DeleteService(service))
    {
        PLOG(ERROR) << "DeleteService failed";
        return false;
    }

    service.reset();
    sc_manager.reset();

    static const int kMaxAttempts = 15;
    static const int kAttemptInterval = 100; // ms

    for (int i = 0; i < kMaxAttempts; ++i)
    {
        if (!isInstalled(name))
            return true;

        Sleep(kAttemptInterval);
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerWin::isInstalled(const QString& name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return false;
    }

    ScopedScHandle service(OpenServiceW(sc_manager, qUtf16Printable(name), SERVICE_QUERY_CONFIG));
    if (!service.isValid())
    {
        if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            PLOG(ERROR) << "OpenServiceW failed";
        }

        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool ServiceControllerWin::isRunning(const QString& name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return false;
    }

    ScopedScHandle service(OpenServiceW(sc_manager, qUtf16Printable(name), SERVICE_QUERY_STATUS));
    if (!service.isValid())
    {
        PLOG(ERROR) << "OpenServiceW failed";
        return false;
    }

    SERVICE_STATUS status;
    if (!QueryServiceStatus(service, &status))
    {
        PLOG(ERROR) << "QueryServiceStatus failed";
        return false;
    }

    return status.dwCurrentState != SERVICE_STOPPED;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::setDescription(const QString& description)
{
    SERVICE_DESCRIPTIONW service_description;
    service_description.lpDescription = const_cast<LPWSTR>(qUtf16Printable(description));

    // Set the service description.
    if (!ChangeServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, &service_description))
    {
        PLOG(ERROR) << "ChangeServiceConfig2W failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerWin::description() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(FATAL) << "QueryServiceConfig2W: unexpected result";
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);

    if (!QueryServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, buffer.get(), bytes_needed,
                             &bytes_needed))
    {
        PLOG(ERROR) << "QueryServiceConfig2W failed";
        return QString();
    }

    SERVICE_DESCRIPTION* service_description =
        reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.get());
    if (!service_description->lpDescription)
        return QString();

    return QString::fromWCharArray(service_description->lpDescription);
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::setDependencies(const QStringList& dependencies)
{
    QByteArray buffer;

    for (auto it = dependencies.constBegin(); it != dependencies.constEnd(); ++it)
    {
        const QString& str = *it;

        buffer += QByteArray(reinterpret_cast<const char*>(str.utf16()),
                             (str.length() + 1) * sizeof(wchar_t));
    }

    buffer.append(static_cast<char>(0));
    buffer.append(static_cast<char>(0));

    if (!ChangeServiceConfigW(service_, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
        nullptr, nullptr, nullptr, reinterpret_cast<const wchar_t*>(buffer.data()), nullptr,
        nullptr, nullptr))
    {
        PLOG(ERROR) << "ChangeServiceConfigW failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QStringList ServiceControllerWin::dependencies() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfigW(service_, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(FATAL) << "QueryServiceConfigW: unexpected result";
        return QStringList();
    }

    if (!bytes_needed)
        return QStringList();

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);
    QUERY_SERVICE_CONFIGW* service_config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.get());

    if (!QueryServiceConfigW(service_, service_config, bytes_needed, &bytes_needed))
    {
        PLOG(ERROR) << "QueryServiceConfigW failed";
        return QStringList();
    }

    if (!service_config->lpDependencies)
        return QStringList();

    QStringList list;
    size_t len = 0;
    for (;;)
    {
        QString str = QString::fromWCharArray(service_config->lpDependencies + len);

        len += str.length() + 1;

        if (str.isEmpty())
            break;

        list.append(str);
    }

    return list;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::setAccount(const QString& username, const QString& password,
                                      const QStringList& paths)
{
    // Grant the account access to its directories before switching to it, so a failure here leaves
    // the service on its previous account. The per-service virtual account is managed by the SCM,
    // so only the file access has to be provisioned.
    for (const QString& path : paths)
    {
        QDir().mkpath(path);

        if (!grantFileAccess(username, path))
        {
            LOG(ERROR) << "Failed to grant access to" << path << "for" << username;
            return false;
        }
    }

    // Virtual service accounts ("NT SERVICE\<name>") have no password: ChangeServiceConfigW
    // requires nullptr for them, not an empty string.
    if (!ChangeServiceConfigW(service_, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
        nullptr, nullptr, nullptr, nullptr, qUtf16Printable(username),
        password.isEmpty() ? nullptr : qUtf16Printable(password), nullptr))
    {
        PLOG(ERROR) << "ChangeServiceConfigW failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString ServiceControllerWin::filePath() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfigW(service_, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(FATAL) << "QueryServiceConfigW: unexpected result";
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);
    QUERY_SERVICE_CONFIGW* service_config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.get());

    if (!QueryServiceConfigW(service_, service_config, bytes_needed, &bytes_needed))
    {
        PLOG(ERROR) << "QueryServiceConfigW failed";
        return QString();
    }

    if (!service_config->lpBinaryPathName)
        return QString();

    return QString::fromWCharArray(service_config->lpBinaryPathName);
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::isRunning() const
{
    SERVICE_STATUS status;

    if (!QueryServiceStatus(service_, &status))
    {
        PLOG(ERROR) << "QueryServiceStatus failed";
        return false;
    }

    return status.dwCurrentState != SERVICE_STOPPED;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::start()
{
    if (!StartServiceW(service_, 0, nullptr))
    {
        PLOG(ERROR) << "StartServiceW failed";
        return false;
    }

    SERVICE_STATUS status;
    if (QueryServiceStatus(service_, &status))
    {
        bool is_started = status.dwCurrentState == SERVICE_RUNNING;
        int number_of_attempts = 0;

        while (!is_started && number_of_attempts < 15)
        {
            Sleep(250);

            if (!QueryServiceStatus(service_, &status))
                break;

            is_started = status.dwCurrentState == SERVICE_RUNNING;
            ++number_of_attempts;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ServiceControllerWin::stop()
{
    SERVICE_STATUS status;

    if (!ControlService(service_, SERVICE_CONTROL_STOP, &status))
    {
        PLOG(ERROR) << "ControlService failed";
        return false;
    }

    bool is_stopped = status.dwCurrentState == SERVICE_STOPPED;
    int number_of_attempts = 0;

    while (!is_stopped && number_of_attempts < 15)
    {
        Sleep(250);

        if (!QueryServiceStatus(service_, &status))
            break;

        is_stopped = status.dwCurrentState == SERVICE_STOPPED;
        ++number_of_attempts;
    }

    return is_stopped;
}
