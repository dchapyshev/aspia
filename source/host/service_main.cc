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

#include "host/service_main.h"

#include <QCommandLineParser>
#include <QIODevice>

#include "base/application.h"
#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/service_controller.h"
#include "build/version.h"
#include "host/host_utils.h"
#include "host/host_storage.h"
#include "host/service.h"

namespace host {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<base::ServiceController> controller =
        base::ServiceController::open(host::Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->start())
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
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::open(host::Service::kName);
    if (!controller)
    {
        out << "Failed to access the service. Not enough rights or service not installed." << Qt::endl;
        return 1;
    }

    if (!controller->stop())
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
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::install(
        host::Service::kName, host::Service::kDisplayName, base::Application::applicationFilePath());
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(host::Service::kDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (base::ServiceController::isRunning(host::Service::kName))
        stopService(out);

    if (!base::ServiceController::remove(host::Service::kName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int hostServiceMain(int& argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

    if (!HostUtils::integrityCheck())
    {
        LOG(ERROR) << "Integrity check failed. Application stopped";
        return 1;
    }

    host::HostUtils::printDebugInfo();

    QCommandLineOption install_option("install",
        base::Application::translate("ServiceMain", "Install service."));
    QCommandLineOption remove_option("remove",
        base::Application::translate("ServiceMain", "Remove service."));
    QCommandLineOption start_option("start",
        base::Application::translate("ServiceMain", "Start service."));
    QCommandLineOption stop_option("stop",
        base::Application::translate("ServiceMain", "Stop service."));
    QCommandLineOption hostid_option("host-id",
        base::Application::translate("ServiceMain", "Get current host id."));

    QCommandLineParser parser;
    parser.addOption(install_option);
    parser.addOption(remove_option);
    parser.addOption(start_option);
    parser.addOption(stop_option);
    parser.addOption(hostid_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    QTextStream out(stdout, QIODevice::WriteOnly);

    if (parser.isSet(hostid_option))
    {
        HostStorage storage;
        out << storage.lastHostId() << Qt::endl;
        return 0;
    }

    if (parser.isSet(install_option))
        return installService(out);
    else if (parser.isSet(remove_option))
        return removeService(out);
    else if (parser.isSet(start_option))
        return startService(out);
    else if (parser.isSet(stop_option))
        return stopService(out);

    return Service().exec(application);
}

} // namespace host
