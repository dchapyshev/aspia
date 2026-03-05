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

#include "host/desktop_agent_main.h"

#include <QCommandLineParser>

#include "base/application.h"
#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "build/version.h"
#include "host/desktop_agent.h"
#include "host/host_utils.h"

//--------------------------------------------------------------------------------------------------
int desktopAgentMain(int& argc, char* argv[])
{
    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    base::Application::setEventDispatcher(new base::AsioEventDispatcher());
    base::Application::setApplicationVersion(ASPIA_VERSION_STRING);
    base::Application application(argc, argv);

    host::HostUtils::printDebugInfo(
        host::HostUtils::INCLUDE_VIDEO_ADAPTERS | host::HostUtils::INCLUDE_WINDOW_STATIONS);

    QCommandLineOption channel_id_option("channel_id",
        base::Application::translate("DesktopAgentMain", "IPC channel id."), "channel_id");

    QCommandLineParser parser;
    parser.addOption(channel_id_option);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(application);

    if (!parser.isSet(channel_id_option))
    {
        LOG(ERROR) << "Parameter channel_id is not specified";
        return 1;
    }

    host::DesktopAgent desktop_agent;
    desktop_agent.start(parser.value(channel_id_option));

    return application.exec();
}
