//
// Aspia Project
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

#include "base/meta_types.h"
#include "base/logging.h"
#include "base/threading/asio_event_dispatcher.h"
#include "build/version.h"
#include "proto/meta_types.h"
#include "relay/service.h"
#include "relay/settings.h"

#if defined(OS_WIN)
#include "relay/win/service_util.h"
#else
#include "relay/controller.h"
#endif

#include <iostream>

#include <QCommandLineParser>

namespace {

//--------------------------------------------------------------------------------------------------
int createConfig()
{
    relay::Settings settings;

    if (!settings.isEmpty())
    {
        std::cout << "Settings file already exists. Continuation is impossible." << std::endl;
        return 1;
    }

    // Save the configuration file.
    settings.reset();
    settings.sync();

    std::cout << "Configuration successfully created." << std::endl;
    return 0;
}

} // namespace

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    base::ScopedLogging logging;

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);

    base::Application application(argc, argv);

    QCommandLineParser parser;

#if defined(Q_OS_WINDOWS)
    QCommandLineOption install_option("install", "Install service.");
    QCommandLineOption remove_option("remove", "Remove service.");
    QCommandLineOption start_option("start", "Start service.");
    QCommandLineOption stop_option("stop", "Stop service.");

    parser.addOption(install_option);
    parser.addOption(remove_option);
    parser.addOption(start_option);
    parser.addOption(stop_option);
#endif //defined(Q_OS_WINDOWS)

    QCommandLineOption create_config_option("create-config", "Creates a configuration.");

    parser.addOption(create_config_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING << " (arch: " << ARCH_CPU_STRING << ")";
    LOG(LS_INFO) << "Command line: " << base::Application::arguments();

    if (parser.isSet(create_config_option))
    {
        return createConfig();
    }
#if defined(Q_OS_WINDOWS)
    else if (parser.isSet(install_option))
    {
        return relay::installService();
    }
    else if (parser.isSet(remove_option))
    {
        return relay::removeService();
    }
    else if (parser.isSet(start_option))
    {
        return relay::startService();
    }
    else if (parser.isSet(stop_option))
    {
        return relay::stopService();
    }
#endif // defined(Q_OS_WINDOWS)
    else
    {
        base::registerMetaTypes();
        proto::registerMetaTypes();

        return relay::Service().exec(application);
    }
}
