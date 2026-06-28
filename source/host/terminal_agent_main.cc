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

#include "host/terminal_agent_main.h"

#include "base/core_application.h"
#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/ipc/ipc_server.h"
#include "build/version.h"
#include "host/terminal_agent.h"
#include "host/host_utils.h"

//--------------------------------------------------------------------------------------------------
int terminalAgentMain(int& argc, char* argv[])
{
    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;

    ScopedLogging scoped_logging(logging_settings);

    CoreApplication::setEventDispatcher(new AsioEventDispatcher());
    CoreApplication::setApplicationVersion(ASPIA_VERSION_STRING);

    CoreApplication application(argc, argv);
    HostUtils::printDebugInfo();

    QString channel_id = qEnvironmentVariable(IpcServer::kChannelIdEnvVar);
    if (channel_id.isEmpty())
    {
        LOG(ERROR) << "Environment variable" << IpcServer::kChannelIdEnvVar << "is not set";
        return 1;
    }

    TerminalAgent terminal_agent;
    terminal_agent.start(channel_id);

    return application.exec();
}
