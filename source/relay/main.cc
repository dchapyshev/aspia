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

#include <QCommandLineParser>
#include <QIODevice>
#include <QSysInfo>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/service_controller.h"
#include "build/version.h"
#include "relay/service.h"
#include "relay/settings.h"

namespace {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::open(relay::Service::kName);
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
    std::unique_ptr<base::ServiceController> controller = base::ServiceController::open(relay::Service::kName);
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
        relay::Service::kName, relay::Service::kDisplayName, QCoreApplication::applicationFilePath());
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(relay::Service::kDescription);
    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (base::ServiceController::isRunning(relay::Service::kName))
        stopService(out);

    if (!base::ServiceController::remove(relay::Service::kName))
    {
        out << "Failed to remove the service." << Qt::endl;
        return 1;
    }

    out << "The service was successfully deleted." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int createConfig(QTextStream& out)
{
    relay::Settings settings;

    if (!settings.isEmpty())
    {
        out << "Settings file already exists. Continuation is impossible." << Qt::endl;
        return 1;
    }

    // Save the configuration file.
    settings.reset();
    settings.sync();

    out << "Configuration successfully created." << Qt::endl;
    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

    QCommandLineOption create_config_option("create-config", "Creates a configuration.");
    QCommandLineOption install_option("install", "Install service.");
    QCommandLineOption remove_option("remove", "Remove service.");
    QCommandLineOption start_option("start", "Start service.");
    QCommandLineOption stop_option("stop", "Stop service.");

    QCommandLineParser parser;
    parser.addOption(create_config_option);
    parser.addOption(install_option);
    parser.addOption(remove_option);
    parser.addOption(start_option);
    parser.addOption(stop_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING << "(arch:" << QSysInfo::buildCpuArchitecture() << ")";
    LOG(INFO) << "Command line:" << base::Application::arguments();

    QTextStream out(stdout, QIODevice::WriteOnly);

    if (parser.isSet(create_config_option))
        return createConfig(out);
    else if (parser.isSet(install_option))
        return installService(out);
    else if (parser.isSet(remove_option))
        return removeService(out);
    else if (parser.isSet(start_option))
        return startService(out);
    else if (parser.isSet(stop_option))
        return stopService(out);

    return relay::Service().exec(application);
}
