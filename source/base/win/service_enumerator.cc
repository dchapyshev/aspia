//
// SmartCafe Project
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

#include "base/win/service_enumerator.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ServiceEnumerator::ServiceEnumerator(Type type)
{
    manager_handle_.reset(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!manager_handle_.isValid())
    {
        PLOG(ERROR) << "OpenSCManagerW failed";
        return;
    }

    DWORD bytes_needed = 0;
    DWORD resume_handle = 0;

    if (EnumServicesStatusExW(manager_handle_,
                              SC_ENUM_PROCESS_INFO,
                              type == Type::SERVICES ? SERVICE_WIN32 : SERVICE_DRIVER,
                              SERVICE_STATE_ALL,
                              nullptr,
                              0,
                              &bytes_needed,
                              &services_count_,
                              &resume_handle,
                              nullptr)
        || GetLastError() != ERROR_MORE_DATA)
    {
        PLOG(ERROR) << "Unexpected return value";
        return;
    }

    services_buffer_ = std::make_unique<quint8[]>(bytes_needed);

    if (!EnumServicesStatusExW(manager_handle_,
                               SC_ENUM_PROCESS_INFO,
                               type == Type::SERVICES ? SERVICE_WIN32 : SERVICE_DRIVER,
                               SERVICE_STATE_ALL,
                               services_buffer_.get(),
                               bytes_needed,
                               &bytes_needed,
                               &services_count_,
                               &resume_handle,
                               nullptr))
    {
        PLOG(ERROR) << "EnumServicesStatusExW failed";
        services_buffer_.reset();
        services_count_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
bool ServiceEnumerator::isAtEnd() const
{
    return current_service_index_ >= services_count_;
}

//--------------------------------------------------------------------------------------------------
void ServiceEnumerator::advance()
{
    current_service_handle_.reset();
    current_service_config_.reset();
    ++current_service_index_;
}

//--------------------------------------------------------------------------------------------------
ENUM_SERVICE_STATUS_PROCESS* ServiceEnumerator::currentService() const
{
    if (!services_buffer_ || !services_count_ || isAtEnd())
        return nullptr;

    ENUM_SERVICE_STATUS_PROCESS* services =
        reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(services_buffer_.get());

    return &services[current_service_index_];
}

//--------------------------------------------------------------------------------------------------
SC_HANDLE ServiceEnumerator::currentServiceHandle() const
{
    if (!current_service_handle_.isValid())
    {
        ENUM_SERVICE_STATUS_PROCESS* services =
            reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS*>(services_buffer_.get());

        const DWORD desired_access =
            SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS;

        current_service_handle_.reset(OpenServiceW(manager_handle_,
                                                   services[current_service_index_].lpServiceName,
                                                   desired_access));
    }

    return current_service_handle_;
}

//--------------------------------------------------------------------------------------------------
LPQUERY_SERVICE_CONFIG ServiceEnumerator::currentServiceConfig() const
{
    if (!current_service_config_)
    {
        SC_HANDLE service_handle = currentServiceHandle();

        DWORD bytes_needed = 0;

        if (QueryServiceConfigW(service_handle,
                                nullptr,
                                0,
                                &bytes_needed)
            || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            PLOG(ERROR) << "QueryServiceConfigW failed";
            return nullptr;
        }

        current_service_config_ = std::make_unique<quint8[]>(bytes_needed);

        if (!QueryServiceConfigW(service_handle,
                                 reinterpret_cast<LPQUERY_SERVICE_CONFIG>(
                                     current_service_config_.get()),
                                 bytes_needed,
                                 &bytes_needed))
        {
            PLOG(ERROR) << "QueryServiceConfigW failed";
            return nullptr;
        }
    }

    return reinterpret_cast<LPQUERY_SERVICE_CONFIG>(current_service_config_.get());
}

//--------------------------------------------------------------------------------------------------
QString ServiceEnumerator::name() const
{
    ENUM_SERVICE_STATUS_PROCESS* service = currentService();

    if (!service || !service->lpServiceName)
        return QString();

    return QString::fromWCharArray(service->lpServiceName);
}

//--------------------------------------------------------------------------------------------------
QString ServiceEnumerator::displayName() const
{
    ENUM_SERVICE_STATUS_PROCESS* service = currentService();

    if (!service || !service->lpDisplayName)
        return QString();

    return QString::fromWCharArray(service->lpDisplayName);
}

//--------------------------------------------------------------------------------------------------
QString ServiceEnumerator::description() const
{
    SC_HANDLE service_handle = currentServiceHandle();

    DWORD bytes_needed = 0;

    if (QueryServiceConfig2W(service_handle,
                             SERVICE_CONFIG_DESCRIPTION,
                             nullptr,
                             0,
                             &bytes_needed)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        PLOG(ERROR) << "QueryServiceConfig2W failed";
        return QString();
    }

    std::unique_ptr<quint8[]> result = std::make_unique<quint8[]>(bytes_needed);

    if (!QueryServiceConfig2W(service_handle,
                              SERVICE_CONFIG_DESCRIPTION,
                              result.get(),
                              bytes_needed,
                              &bytes_needed))
    {
        PLOG(ERROR) << "QueryServiceConfig2W failed";
        return QString();
    }

    SERVICE_DESCRIPTION* description = reinterpret_cast<SERVICE_DESCRIPTION*>(result.get());
    if (!description->lpDescription)
        return QString();

    return QString::fromWCharArray(description->lpDescription);
}

//--------------------------------------------------------------------------------------------------
ServiceEnumerator::Status ServiceEnumerator::status() const
{
    ENUM_SERVICE_STATUS_PROCESS* service = currentService();

    if (!service)
        return Status::UNKNOWN;

    switch (service->ServiceStatusProcess.dwCurrentState)
    {
        case SERVICE_CONTINUE_PENDING:
            return Status::CONTINUE_PENDING;

        case SERVICE_PAUSE_PENDING:
            return Status::PAUSE_PENDING;

        case SERVICE_PAUSED:
            return Status::PAUSED;

        case SERVICE_RUNNING:
            return Status::RUNNING;

        case SERVICE_START_PENDING:
            return Status::START_PENDING;

        case SERVICE_STOP_PENDING:
            return Status::STOP_PENDING;

        case SERVICE_STOPPED:
            return Status::STOPPED;

        default:
            return Status::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
ServiceEnumerator::StartupType ServiceEnumerator::startupType() const
{
    LPQUERY_SERVICE_CONFIG config = currentServiceConfig();

    if (!config)
        return StartupType::UNKNOWN;

    switch (config->dwStartType)
    {
        case SERVICE_AUTO_START:
            return StartupType::AUTO_START;

        case SERVICE_DEMAND_START:
            return StartupType::DEMAND_START;

        case SERVICE_DISABLED:
            return StartupType::DISABLED;

        case SERVICE_BOOT_START:
            return StartupType::BOOT_START;

        case SERVICE_SYSTEM_START:
            return StartupType::SYSTEM_START;

        default:
            return StartupType::UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
QString ServiceEnumerator::binaryPath() const
{
    LPQUERY_SERVICE_CONFIG config = currentServiceConfig();

    if (!config || !config->lpBinaryPathName)
        return QString();

    return QString::fromWCharArray(config->lpBinaryPathName);
}

//--------------------------------------------------------------------------------------------------
QString ServiceEnumerator::startName() const
{
    LPQUERY_SERVICE_CONFIG config = currentServiceConfig();

    if (!config || !config->lpServiceStartName)
        return QString();

    return QString::fromWCharArray(config->lpServiceStartName);
}

} // namespace base
