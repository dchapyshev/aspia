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

#include "relay/win/service_util.h"

#include "base/win/service_controller.h"
#include "relay/service_constants.h"

#include <QCoreApplication>
#include <QTextStream>

namespace relay {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    base::ServiceController controller = base::ServiceController::open(relay::kServiceName);
    if (!controller.isValid())
    {
        out << "Failed to access the service. Not enough rights or service not installed."
            << Qt::endl;
        return 1;
    }

    if (!controller.start())
    {
        out << "Failed to start the service." << Qt::endl;
        return 1;
    }

    out << "The service started successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int stopService(QTextStream& out)
{
    base::ServiceController controller = base::ServiceController::open(relay::kServiceName);
    if (!controller.isValid())
    {
        out << "Failed to access the service. Not enough rights or service not installed."
            << Qt::endl;
        return 1;
    }

    if (!controller.stop())
    {
        out << "Failed to stop the service." << Qt::endl;
        return 1;
    }

    out << "The service has stopped successfully." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int installService(QTextStream& out)
{
    base::ServiceController controller = base::ServiceController::install(
        relay::kServiceName, relay::kServiceDisplayName, QCoreApplication::applicationFilePath());
    if (!controller.isValid())
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller.setDescription(relay::kServiceDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (base::ServiceController::isRunning(relay::kServiceName))
        stopService(out);

    if (!base::ServiceController::remove(relay::kServiceName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

} // namespace relay
