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

#include "base/logging.h"

namespace base {

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
    const QString& name, const QString& display_name, const QString& file_path)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return nullptr;
    }

    ScopedScHandle service(CreateServiceW(sc_manager, qUtf16Printable(name),
        qUtf16Printable(display_name), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, qUtf16Printable(QDir::toNativeSeparators(file_path)),
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
bool ServiceControllerWin::setAccount(const QString& username, const QString& password)
{
    if (!ChangeServiceConfigW(service_, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
        nullptr, nullptr, nullptr, nullptr, qUtf16Printable(username), qUtf16Printable(password),
        nullptr))
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

} // namespace base
