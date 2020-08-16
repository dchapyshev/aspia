//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/base_paths.h"

#if defined(OS_WIN)
#include "base/win/service_controller.h"
#include "router/win/service.h"
#include "router/win/service_constants.h"
#endif // defined(OS_WIN)

#if defined(USE_TBB)
#include <tbb/tbbmalloc_proxy.h>
#endif // defined(USE_TBB)

#include <iostream>

namespace {

void initLogging()
{
    std::filesystem::path path;

#if defined(OS_WIN)
    if (!base::BasePaths::commonAppData(&path))
        return;
    path.append("Aspia/Logs");
#else
#error Not implemented
#endif

    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = path;

    base::initLogging(settings);
}

void shutdownLogging()
{
    base::shutdownLogging();
}

void startService()
{
    base::win::ServiceController controller =
        base::win::ServiceController::open(router::kServiceName);
    if (!controller.isValid())
    {
        std::cout << "Failed to access the service. Not enough rights or service not installed."
            << std::endl;
    }
    else
    {
        if (!controller.start())
        {
            std::cout << "Failed to start the service." << std::endl;
        }
        else
        {
            std::cout << "The service started successfully." << std::endl;
        }
    }
}

void stopService()
{
    base::win::ServiceController controller =
        base::win::ServiceController::open(router::kServiceName);
    if (!controller.isValid())
    {
        std::cout << "Failed to access the service. Not enough rights or service not installed."
            << std::endl;
    }
    else
    {
        if (!controller.stop())
        {
            std::cout << "Failed to stop the service." << std::endl;
        }
        else
        {
            std::cout << "The service has stopped successfully." << std::endl;
        }
    }
}

void installService()
{
    std::filesystem::path file_path;

    if (!base::BasePaths::currentExecFile(&file_path))
    {
        std::cout << "Failed to get the path to the executable." << std::endl;
    }
    else
    {
        base::win::ServiceController controller = base::win::ServiceController::install(
            router::kServiceName, router::kServiceDisplayName, file_path);
        if (!controller.isValid())
        {
            std::cout << "Failed to install the service." << std::endl;
        }
        else
        {
            controller.setDescription(router::kServiceDescription);
            std::cout << "The service has been successfully installed." << std::endl;
        }
    }
}

void removeService()
{
    if (base::win::ServiceController::isRunning(router::kServiceName))
    {
        stopService();
    }

    if (!base::win::ServiceController::remove(router::kServiceName))
    {
        std::cout << "Failed to remove the service." << std::endl;
    }
    else
    {
        std::cout << "The service was successfully deleted." << std::endl;
    }
}

void showHelp()
{
    std::cout << "aspia_router [switch]" << std::endl
        << "Available switches:" << std::endl
        << '\t' << "--install" << '\t' << "Install service" << std::endl
        << '\t' << "--remove" << '\t' << "Remove service" << std::endl
        << '\t' << "--start" << '\t' << "Start service" << std::endl
        << '\t' << "--stop" << '\t' << "Stop service" << std::endl
        << '\t' << "--help" << '\t' << "Show help" << std::endl;
}

void entryPoint(int argc, const char* const* argv)
{
    initLogging();

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    if (command_line->hasSwitch(u"install"))
    {
        installService();
    }
    else if (command_line->hasSwitch(u"remove"))
    {
        removeService();
    }
    else if (command_line->hasSwitch(u"start"))
    {
        startService();
    }
    else if (command_line->hasSwitch(u"stop"))
    {
        stopService();
    }
    else if (command_line->hasSwitch(u"help"))
    {
        showHelp();
    }
    else
    {
        router::Service().exec();
    }

    shutdownLogging();
}

} // namespace

int wmain()
{
    entryPoint(0, nullptr); // On Windows ignores arguments.
    return 0;
}
