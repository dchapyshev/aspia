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
#include "base/files/base_paths.h"
#include "build/version.h"
#include "relay/service.h"
#include "relay/settings.h"

namespace {

//--------------------------------------------------------------------------------------------------
int startService(QTextStream& out)
{
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
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
    std::unique_ptr<ServiceController> controller = ServiceController::open(Service::kName);
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
    // The service is never installed without a configuration. On a clean system there is none yet,
    // so this is a no-op (the package install must not fail); the user creates the configuration
    // first and runs --install afterwards. On an upgrade the existing configuration is present, so
    // the service is reinstalled and its parameters refreshed.
    Settings settings;
    if (settings.isEmpty())
    {
        out << "Configuration does not exist; the service was not installed. Create it with "
               "--create-config, then run --install." << Qt::endl;
        return 0;
    }

    std::unique_ptr<ServiceController> controller = ServiceController::install(
        Service::kName, Service::kDisplayName, QCoreApplication::applicationFilePath());
    if (!controller)
    {
        out << "Failed to install the service." << Qt::endl;
        return 1;
    }

    controller->setDescription(Service::kDescription);

#if defined(Q_OS_WINDOWS)
    // Start only after the network stack is up.
    controller->setDependencies({ "RpcSs", "Tcpip", "NDIS", "AFD" });
#endif

    // Run the service under its low-privilege account with access only to the directory it needs
    // (config files).
    const QString account = ServiceController::lowPrivilegeAccount(Service::kName);
    if (!account.isEmpty() &&
        !controller->setAccount(account, QString(), { BasePaths::appConfigDir() }))
    {
        out << "Warning: failed to reduce service privileges. The service runs under the default "
               "system account." << Qt::endl;
    }

    out << "The service has been successfully installed." << Qt::endl;
    return 0;
}

//--------------------------------------------------------------------------------------------------
int removeService(QTextStream& out)
{
    if (ServiceController::isRunning(Service::kName))
        stopService(out);

    if (!ServiceController::remove(Service::kName))
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
    Settings settings;

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
    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;
#if defined(Q_OS_WINDOWS)
    // The service runs under a low-privilege account that can only write to its own directories, so
    // place the logs next to the application data instead of the system temp directory.
    logging_settings.log_dir = BasePaths::appConfigDir() + "/logs";
#endif

    ScopedLogging scoped_logging(logging_settings);

    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);

    CoreApplication application(argc, argv);

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
    LOG(INFO) << "Command line:" << CoreApplication::arguments();

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

    return Service().exec(application);
}
