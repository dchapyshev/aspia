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

#include "host/desktop_agent_main.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "host/desktop_session_agent.h"

void desktopAgentMain(int argc, const char* const* argv)
{
    base::initLogging();

    base::CommandLine::init(argc, argv);
    base::CommandLine* command_line = base::CommandLine::forCurrentProcess();

    if (command_line->hasSwitch(u"channel_id"))
    {
        std::unique_ptr<base::MessageLoop> message_loop =
            std::make_unique<base::MessageLoop>(base::MessageLoop::Type::ASIO);

        std::shared_ptr<host::DesktopSessionAgent> desktop_agent =
            std::make_shared<host::DesktopSessionAgent>(message_loop->taskRunner());

        desktop_agent->start(command_line->switchValue(u"channel_id"));
        message_loop->run();
    }
    else
    {
        LOG(LS_ERROR) << "Parameter channel_id is not specified";
    }

    base::shutdownLogging();
}
