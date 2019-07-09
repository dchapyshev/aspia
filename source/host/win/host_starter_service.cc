//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/win/host_starter_service.h"

#include "base/service_controller.h"
#include "base/win/process.h"
#include "build/build_config.h"
#include "host/win/host_process.h"
#include "qt_base/qt_logging.h"

#include <random>

namespace host {

namespace {

const char kServiceName[] = "aspia-host-launcher";
const char kServiceDisplayName[] = "Aspia Host Launcher";

QString createUniqueId(const QString& session_id)
{
    static std::atomic_uint32_t last_service_id = 0;
    uint32_t service_id = last_service_id++;

    std::random_device random_device;
    std::mt19937 engine(random_device());

    std::uniform_int_distribution<> uniform_distance(std::numeric_limits<uint32_t>::min(),
                                                     std::numeric_limits<uint32_t>::max());
    uint32_t random_number = uniform_distance(engine);

    return QString("%1.%2.%3")
        .arg(service_id)
        .arg(random_number)
        .arg(session_id);
}

QString createName(const QString& name, const QString& unique_id)
{
    return name + QLatin1Char('.') + unique_id;
}

} // namespace

StarterService::StarterService(const QString& service_id)
    : base::Service<QCoreApplication>(createName(QLatin1String(kServiceName), service_id),
                                      QLatin1String(kServiceDisplayName),
                                      QString())

{
    // Nothing
}

StarterService::~StarterService() = default;

// static
bool StarterService::startFromService(const QString& program,
                                      const QStringList& arguments,
                                      const QString& session_id)
{
    // Generate a unique identifier for the service.
    QString service_id = createUniqueId(session_id);

    // The service must have a unique name.
    QString unique_name = createName(kServiceName, service_id);

    QStringList service_arguments;
    service_arguments << arguments;
    service_arguments << QLatin1String("--service_id") << service_id;

    // Create a command line to start the service.
    QString command_line = base::win::Process::createCommandLine(program, service_arguments);

    // Install the service in the system.
    base::ServiceController controller = base::ServiceController::install(
        unique_name, QLatin1String(kServiceDisplayName), command_line);
    if (!controller.isValid())
        return false;

    // We are trying to start the service.
    if (!controller.start())
    {
        // Could not start service. Close the controller.
        controller.close();

        // Remove service.
        base::ServiceController::remove(unique_name);
        return false;
    }

    return true;
}

void StarterService::start()
{
    QStringList arguments = QCoreApplication::arguments();
    QStringList service_id;

    const int index = arguments.indexOf(QLatin1String("--service_id"));
    if (index != -1)
    {
        // Remove argument.
        arguments.removeAt(index);

        if (index < arguments.size())
        {
            // Get parts of service id.
            service_id = arguments.at(index).split(QLatin1Char('.'));

            // Remove value.
            arguments.removeAt(index);
        }
    }

    if (service_id.size() != 3)
    {
        LOG(LS_ERROR) << "Service ID has invalid format";
    }
    else
    {
        bool ok;
        base::win::SessionId session_id = service_id.last().toULong(&ok);
        if (!ok)
        {
            LOG(LS_ERROR) << "Service ID has invalid format";
        }
        else
        {
            if (session_id != base::win::kInvalidSessionId &&
                session_id != base::win::kServiceSessionId)
            {
                // Start the session process.
                HostProcess process;
                process.setProgram(QCoreApplication::applicationFilePath());
                process.setArguments(arguments);
                process.setSessionId(session_id);
                process.setAccount(HostProcess::User);
                process.start();
            }
        }
    }

    // Remove service.
    if (!base::ServiceController::remove(serviceName()))
    {
        LOG(LS_ERROR) << "Could not remove service";
    }

    // Completing the application.
    QCoreApplication::quit();
}

void StarterService::stop()
{
    // Nothing
}

void StarterService::sessionEvent(base::win::SessionStatus /* status */,
                                  base::win::SessionId /* session_id */)
{
    // Nothing
}

} // namespace host
