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

#include "host/workers/tools_worker.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/desktop_tools.h"

namespace {

// Minimal interval between two launches.
const MilliSeconds kMinLaunchInterval { 500 };

} // namespace

//--------------------------------------------------------------------------------------------------
ToolsWorker::ToolsWorker()
    : Worker(Thread::AsioDispatcher)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ToolsWorker::~ToolsWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::query(QObject* context, SessionId session_id, const QByteArray& buffer,
                        std::function<void(QByteArray)> reply)
{
    Worker::request(context, [this, session_id, buffer]()
    {
        proto::tools::ClientToHost message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse tools request";
            return QByteArray();
        }

        return readMessage(session_id, message);
    },
    std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::requestToolList(QObject* context, SessionId session_id,
                                  std::function<void(QByteArray)> reply)
{
    Worker::request(context, [this, session_id]()
    {
        return toolList(session_id);
    },
    std::move(reply));
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onStart()
{
    LOG(INFO) << "Tools worker started";

    buildToolList();
    buildScriptList();

    LOG(INFO) << "Lists built (tools:" << tools_.size() << "scripts:" << scripts_.size() << ")";
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onStop()
{
    LOG(INFO) << "Tools worker stopped";

    tools_.clear();
    scripts_.clear();
}

//--------------------------------------------------------------------------------------------------
QByteArray ToolsWorker::readMessage(SessionId session_id, const proto::tools::ClientToHost& message)
{
    if (message.has_tool_list_request())
        return toolList(session_id);

    if (message.has_execute_tool())
        executeTool(session_id, message.execute_tool().id());
    else if (message.has_execute_script())
        executeScript(session_id, message.execute_script().id());
    else
        LOG(WARNING) << "Unhandled tools message";

    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
QByteArray ToolsWorker::toolList(SessionId session_id)
{
    proto::tools::HostToClient message;
    proto::tools::ToolList* tool_list = message.mutable_tool_list();

    // The list stays empty while there is no interactive user in the session: a tool started then
    // has no visible desktop to appear on. For an empty list the client shows nothing.
    if (hasInteractiveUser(session_id))
    {
        for (int i = 0; i < tools_.size(); ++i)
        {
            const Tool& tool = tools_.at(i);

            proto::tools::Tool* item = tool_list->add_tool();
            item->set_id(i);
            item->set_name(tool.name.toStdString());

            if (!tool.icon.isEmpty())
                item->set_icon(tool.icon.data(), tool.icon.size());
        }

        for (int i = 0; i < scripts_.size(); ++i)
        {
            const Script& script = scripts_.at(i);

            proto::tools::Script* item = tool_list->add_script();
            item->set_id(i);
            item->set_name(script.name.toStdString());
            item->set_description(script.description.toStdString());
            item->set_confirm(script.confirm);
        }
    }

    LOG(INFO) << "Sending tool list (tools:" << tool_list->tool_size()
              << "scripts:" << tool_list->script_size() << "sid" << session_id << ")";
    return serialize(message);
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::executeTool(SessionId session_id, qint32 id)
{
    // The client selects a tool by its position in the list we sent it: anything outside the list is
    // never started, whatever the client asks for.
    if (id < 0 || id >= tools_.size())
    {
        LOG(ERROR) << "Request to start an unknown tool:" << id;
        return;
    }

    const Tool& tool = tools_.at(id);

    const TimePoint now = Clock::now();
    if (now - last_launch_time_ < kMinLaunchInterval)
    {
        LOG(WARNING) << "Too many launch requests (tool:" << tool.name << ")";
        return;
    }

    last_launch_time_ = now;

    if (!hasInteractiveUser(session_id))
    {
        LOG(ERROR) << "No interactive user in session" << session_id;
        return;
    }

    if (!launchTool(session_id, tool))
        LOG(ERROR) << "Unable to start tool" << tool.name << "in session" << session_id;
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::executeScript(SessionId session_id, qint32 id)
{
    if (id < 0 || id >= scripts_.size())
    {
        LOG(ERROR) << "Request to start an unknown script:" << id;
        return;
    }

    const Script& script = scripts_.at(id);

    const TimePoint now = Clock::now();
    if (now - last_launch_time_ < kMinLaunchInterval)
    {
        LOG(WARNING) << "Too many launch requests (script:" << script.name << ")";
        return;
    }

    last_launch_time_ = now;

    if (!hasInteractiveUser(session_id))
    {
        LOG(ERROR) << "No interactive user in session" << session_id;
        return;
    }

    if (!launchScript(session_id, script))
        LOG(ERROR) << "Unable to start script" << script.name << "in session" << session_id;
}
