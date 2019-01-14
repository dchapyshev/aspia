//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/service_controller.h"

#include <memory>

#include "base/logging.h"

namespace base {

ServiceController::ServiceController() = default;

ServiceController::ServiceController(SC_HANDLE sc_manager, SC_HANDLE service)
    : sc_manager_(sc_manager),
      service_(service)
{
    // Nothing
}

ServiceController::ServiceController(ServiceController&& other) noexcept
    : sc_manager_(std::move(other.sc_manager_)),
      service_(std::move(other.service_))
{
    // Nothing
}

ServiceController& ServiceController::operator=(ServiceController&& other) noexcept
{
    sc_manager_ = std::move(other.sc_manager_);
    service_ = std::move(other.service_);
    return *this;
}

ServiceController::~ServiceController() = default;

// static
ServiceController ServiceController::open(const QString& name)
{
    win::ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(LS_WARNING) << "OpenSCManagerW failed";
        return ServiceController();
    }

    win::ScopedScHandle service(OpenServiceW(sc_manager,
                                             qUtf16Printable(name),
                                             SERVICE_ALL_ACCESS));
    if (!service.isValid())
    {
        PLOG(LS_WARNING) << "OpenServiceW failed";
        return ServiceController();
    }

    return ServiceController(sc_manager.release(), service.release());
}

// static
ServiceController ServiceController::install(const QString& name,
                                             const QString& display_name,
                                             const QString& file_path)
{
    win::ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(LS_WARNING) << "OpenSCManagerW failed";
        return ServiceController();
    }

    QString normalized_file_path = file_path;
    normalized_file_path.replace(QLatin1Char('/'), QLatin1Char('\\'));

    win::ScopedScHandle service(CreateServiceW(sc_manager,
                                               qUtf16Printable(name),
                                               qUtf16Printable(display_name),
                                               SERVICE_ALL_ACCESS,
                                               SERVICE_WIN32_OWN_PROCESS,
                                               SERVICE_AUTO_START,
                                               SERVICE_ERROR_NORMAL,
                                               qUtf16Printable(normalized_file_path),
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               nullptr,
                                               nullptr));
    if (!service.isValid())
    {
        PLOG(LS_WARNING) << "CreateServiceW failed";
        return ServiceController();
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
        PLOG(LS_WARNING) << "ChangeServiceConfig2W failed";
        return ServiceController();
    }

    return ServiceController(sc_manager.release(), service.release());
}

// static
bool ServiceController::remove(const QString& name)
{
    win::ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        PLOG(LS_WARNING) << "OpenSCManagerW failed";
        return false;
    }

    win::ScopedScHandle service(OpenServiceW(sc_manager,
                                             qUtf16Printable(name),
                                             SERVICE_ALL_ACCESS));
    if (!service.isValid())
    {
        PLOG(LS_WARNING) << "OpenServiceW failed";
        return false;
    }

    if (!DeleteService(service))
    {
        PLOG(LS_WARNING) << "DeleteService failed";
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

// static
bool ServiceController::isInstalled(const QString& name)
{
    win::ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!sc_manager.isValid())
    {
        PLOG(LS_WARNING) << "OpenSCManagerW failed";
        return false;
    }

    win::ScopedScHandle service(OpenServiceW(sc_manager,
                                             qUtf16Printable(name),
                                             SERVICE_QUERY_CONFIG));
    if (!service.isValid())
    {
        if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            PLOG(LS_WARNING) << "OpenServiceW failed";
        }

        return false;
    }

    return true;
}

void ServiceController::close()
{
    service_.reset();
    sc_manager_.reset();
}

bool ServiceController::setDescription(const QString& description)
{
    SERVICE_DESCRIPTIONW service_description;
    service_description.lpDescription = const_cast<LPWSTR>(qUtf16Printable(description));

    // Set the service description.
    if (!ChangeServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, &service_description))
    {
        PLOG(LS_WARNING) << "ChangeServiceConfig2W failed";
        return false;
    }

    return true;
}

QString ServiceController::description() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(LS_FATAL) << "QueryServiceConfig2W: unexpected result";
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(bytes_needed);

    if (!QueryServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, buffer.get(), bytes_needed,
                             &bytes_needed))
    {
        PLOG(LS_WARNING) << "QueryServiceConfig2W failed";
        return QString();
    }

    SERVICE_DESCRIPTION* service_description =
        reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.get());
    if (!service_description->lpDescription)
        return QString();

    return QString::fromUtf16(reinterpret_cast<const ushort*>(service_description->lpDescription));
}

bool ServiceController::setDependencies(const QStringList& dependencies)
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

    if (!ChangeServiceConfigW(service_,
                              SERVICE_NO_CHANGE,
                              SERVICE_NO_CHANGE,
                              SERVICE_NO_CHANGE,
                              nullptr, nullptr, nullptr,
                              reinterpret_cast<const wchar_t*>(buffer.data()),
                              nullptr, nullptr, nullptr))
    {
        PLOG(LS_WARNING) << "ChangeServiceConfigW failed";
        return false;
    }

    return true;
}

QStringList ServiceController::dependencies() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfigW(service_, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(LS_FATAL) << "QueryServiceConfigW: unexpected result";
        return QStringList();
    }

    if (!bytes_needed)
        return QStringList();

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(bytes_needed);
    QUERY_SERVICE_CONFIGW* service_config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.get());

    if (!QueryServiceConfigW(service_, service_config, bytes_needed, &bytes_needed))
    {
        PLOG(LS_WARNING) << "QueryServiceConfigW failed";
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

QString ServiceController::filePath() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfigW(service_, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(LS_FATAL) << "QueryServiceConfigW: unexpected result";
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(bytes_needed);
    QUERY_SERVICE_CONFIGW* service_config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.get());

    if (!QueryServiceConfigW(service_, service_config, bytes_needed, &bytes_needed))
    {
        PLOG(LS_WARNING) << "QueryServiceConfigW failed";
        return QString();
    }

    if (!service_config->lpBinaryPathName)
        return QString();

    return QString::fromUtf16(reinterpret_cast<const ushort*>(service_config->lpBinaryPathName));
}

bool ServiceController::isValid() const
{
    return sc_manager_.isValid() && service_.isValid();
}

bool ServiceController::isRunning() const
{
    SERVICE_STATUS status;

    if (!QueryServiceStatus(service_, &status))
    {
        PLOG(LS_WARNING) << "QueryServiceStatus failed";
        return false;
    }

    return status.dwCurrentState != SERVICE_STOPPED;
}

bool ServiceController::start()
{
    if (!StartServiceW(service_, 0, nullptr))
    {
        PLOG(LS_WARNING) << "StartServiceW failed";
        return false;
    }

    return true;
}

bool ServiceController::stop()
{
    SERVICE_STATUS status;

    if (!ControlService(service_, SERVICE_CONTROL_STOP, &status))
    {
        PLOG(LS_WARNING) << "ControlService failed";
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
