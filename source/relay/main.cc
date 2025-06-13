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

#include <QCommandLineParser>
#include <QSysInfo>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "build/version.h"
#include "relay/service.h"
#include "relay/settings.h"

#if defined(Q_OS_WINDOWS)
#include "relay/win/service_util.h"
#else
#include "relay/controller.h"
#endif

namespace {

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

    LOG(LS_INFO) << "Version:" << ASPIA_VERSION_STRING
                 << "(arch:" << QSysInfo::buildCpuArchitecture() << ")";
    LOG(LS_INFO) << "Command line:" << base::Application::arguments();

    QTextStream out(stdout, QIODevice::WriteOnly);

    if (parser.isSet(create_config_option))
    {
        return createConfig(out);
    }
#if defined(Q_OS_WINDOWS)
    else if (parser.isSet(install_option))
    {
        return relay::installService(out);
    }
    else if (parser.isSet(remove_option))
    {
        return relay::removeService(out);
    }
    else if (parser.isSet(start_option))
    {
        return relay::startService(out);
    }
    else if (parser.isSet(stop_option))
    {
        return relay::stopService(out);
    }
#endif // defined(Q_OS_WINDOWS)
    else
    {
        return relay::Service().exec(application);
    }
}
