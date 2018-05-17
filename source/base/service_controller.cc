//
// PROJECT:         Aspia
// FILE:            base/service_controller.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_controller.h"

#include <QDebug>
#include <memory>

#include "base/system_error_code.h"

namespace aspia {

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
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        qWarning() << "OpenSCManagerW failed: " << lastSystemErrorString();
        return ServiceController();
    }

    ScopedScHandle service(OpenServiceW(sc_manager,
                                        reinterpret_cast<const wchar_t*>(name.utf16()),
                                        SERVICE_ALL_ACCESS));
    if (!service.isValid())
    {
        qWarning() << "OpenServiceW failed: " << lastSystemErrorString();
        return ServiceController();
    }

    return ServiceController(sc_manager.release(), service.release());
}

// static
ServiceController ServiceController::install(const QString& name,
                                             const QString& display_name,
                                             const QString& file_path)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager.isValid())
    {
        qWarning() << "OpenSCManagerW failed: " << lastSystemErrorString();
        return ServiceController();
    }

    QString normalized_file_path = file_path;
    normalized_file_path.replace(QLatin1Char('/'), QLatin1Char('\\'));

    ScopedScHandle service(CreateServiceW(sc_manager,
                                          reinterpret_cast<const wchar_t*>(name.utf16()),
                                          reinterpret_cast<const wchar_t*>(display_name.utf16()),
                                          SERVICE_ALL_ACCESS,
                                          SERVICE_WIN32_OWN_PROCESS,
                                          SERVICE_AUTO_START,
                                          SERVICE_ERROR_NORMAL,
                                          reinterpret_cast<const wchar_t*>(normalized_file_path.utf16()),
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          nullptr));
    if (!service.isValid())
    {
        qWarning() << "CreateServiceW failed: " << lastSystemErrorString();
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
        qWarning() << "ChangeServiceConfig2W failed: " << lastSystemErrorString();
        return ServiceController();
    }

    return ServiceController(sc_manager.release(), service.release());
}

// static
bool ServiceController::isInstalled(const QString& name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!sc_manager.isValid())
    {
        qWarning() << "OpenSCManagerW failed: " << lastSystemErrorString();
        return false;
    }

    ScopedScHandle service(OpenServiceW(sc_manager,
                                        reinterpret_cast<const wchar_t*>(name.utf16()),
                                        SERVICE_QUERY_STATUS));
    if (!service.isValid())
    {
        if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            qWarning() << "OpenServiceW failed: " << lastSystemErrorString();
        }

        return false;
    }

    return true;
}

bool ServiceController::setDescription(const QString& description)
{
    SERVICE_DESCRIPTIONW service_description;
    service_description.lpDescription =
        const_cast<LPWSTR>(reinterpret_cast<const wchar_t*>(description.utf16()));

    // Set the service description.
    if (!ChangeServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, &service_description))
    {
        qWarning() << "ChangeServiceConfig2W failed: " << lastSystemErrorString();
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
        qWarning("QueryServiceConfig2W: unexpected result");
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);

    if (!QueryServiceConfig2W(service_, SERVICE_CONFIG_DESCRIPTION, buffer.get(), bytes_needed,
                             &bytes_needed))
    {
        qWarning() << "QueryServiceConfig2W failed: " << lastSystemErrorString();
        return QString();
    }

    SERVICE_DESCRIPTION* service_description =
        reinterpret_cast<SERVICE_DESCRIPTION*>(buffer.get());
    if (!service_description->lpDescription)
        return QString();

    return QString::fromUtf16(reinterpret_cast<const ushort*>(service_description->lpDescription));
}

QString ServiceController::filePath() const
{
    DWORD bytes_needed = 0;

    if (QueryServiceConfigW(service_, nullptr, 0, &bytes_needed) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        qWarning("QueryServiceConfigW: unexpected result");
        return QString();
    }

    if (!bytes_needed)
        return QString();

    std::unique_ptr<quint8[]> buffer = std::make_unique<quint8[]>(bytes_needed);
    QUERY_SERVICE_CONFIG* service_config = reinterpret_cast<QUERY_SERVICE_CONFIG*>(buffer.get());

    if (!QueryServiceConfigW(service_, service_config, bytes_needed, &bytes_needed))
    {
        qWarning() << "QueryServiceConfigW failed: " << lastSystemErrorString();
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
        qWarning() << "QueryServiceStatus failed: " << lastSystemErrorString();
        return false;
    }

    return status.dwCurrentState != SERVICE_STOPPED;
}

bool ServiceController::start()
{
    if (!StartServiceW(service_, 0, nullptr))
    {
        qWarning() << "StartServiceW failed: " << lastSystemErrorString();
        return false;
    }

    return true;
}

bool ServiceController::stop()
{
    SERVICE_STATUS status;

    if (!ControlService(service_, SERVICE_CONTROL_STOP, &status))
    {
        qWarning() << "ControlService failed: " << lastSystemErrorString();
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

bool ServiceController::remove()
{
    if (!DeleteService(service_))
    {
        qWarning() << "DeleteService failed: " << lastSystemErrorString();
        return false;
    }

    service_.reset();
    sc_manager_.reset();

    return true;
}

} // namespace aspia
