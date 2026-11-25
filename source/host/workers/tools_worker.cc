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

#include <QFile>

#include "base/logging.h"
#include "base/serialization.h"
#include "host/terminal_process.h"

namespace {

// Minimal interval between two launches.
const MilliSeconds kMinLaunchInterval { 500 };

// Minimal interval between two lists sent at the request of the client.
const MilliSeconds kMinListInterval { 1000 };

//--------------------------------------------------------------------------------------------------
QByteArray scriptResult(qint32 id, proto::tools::ScriptResult::Status status)
{
    proto::tools::HostToClient message;
    proto::tools::ScriptResult* result = message.mutable_script_result();
    result->set_id(id);
    result->set_status(status);
    return serialize(message);
}

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
void ToolsWorker::query(quint32 client_id, SessionId session_id, const QByteArray& buffer)
{
    post([this, client_id, session_id, buffer]()
    {
        proto::tools::ClientToHost message;
        if (!parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse tools request";
            return;
        }

        readMessage(client_id, session_id, message);
    });
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::requestToolList(quint32 client_id, SessionId session_id)
{
    post([this, client_id, session_id]()
    {
        emit sig_message(client_id, toolList(session_id));
    });
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::stopScript(quint32 client_id)
{
    post([this, client_id]()
    {
        if (script_id_ != -1 && script_client_id_ == client_id)
            terminateScript();
    });
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onPrepare()
{
    buildToolList();
    scripts_supported_ = scriptsSupported();

    LOG(INFO) << "Tool list built (tools:" << tools_.size()
              << "scripts:" << (scripts_supported_ ? scriptTable().size() : 0) << ")";
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onStop()
{
    LOG(INFO) << "Tools worker stopped";

    script_process_.reset();
    tools_.clear();
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::onScriptOutput(const QByteArray& data)
{
    proto::tools::HostToClient message;
    proto::tools::ScriptOutput* output = message.mutable_script_output();
    output->set_id(script_id_);
    output->set_data(data.constData(), data.size());
    emit sig_message(script_client_id_, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::readMessage(quint32 client_id, SessionId session_id, const proto::tools::ClientToHost& message)
{
    if (message.has_tool_list_request())
    {
        const TimePoint now = Clock::now();
        if (now - last_list_time_ < kMinListInterval)
        {
            LOG(WARNING) << "Too many tool list requests";
            return;
        }

        last_list_time_ = now;
        emit sig_message(client_id, toolList(session_id));
        return;
    }

    if (message.has_execute_tool())
        executeTool(session_id, message.execute_tool().id());
    else if (message.has_execute_script())
        executeScript(client_id, session_id, message.execute_script());
    else if (message.has_stop_script())
    {
        const proto::tools::StopScript& stop_script = message.stop_script();
        if (script_id_ != -1 && script_id_ == stop_script.id() && script_client_id_ == client_id)
            terminateScript();
    }
    else
        LOG(WARNING) << "Unhandled tools message";
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

        if (scripts_supported_)
        {
            const std::span<const Script> scripts = scriptTable();

            for (size_t i = 0; i < scripts.size(); ++i)
            {
                proto::tools::Script* item = tool_list->add_script();
                item->set_id(static_cast<int>(i));
                item->set_name(scripts[i].name);
                item->set_description(scripts[i].description);
                item->set_confirm(scripts[i].confirm);
            }
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
void ToolsWorker::executeScript(
    quint32 client_id, SessionId session_id, const proto::tools::ExecuteScript& execute_script)
{
    const std::span<const Script> scripts = scriptTable();
    const qint32 id = execute_script.id();

    if (!scripts_supported_ || id < 0 || static_cast<size_t>(id) >= scripts.size())
    {
        LOG(ERROR) << "Request to start an unknown script:" << id;
        return;
    }

    const Script& script = scripts[id];

    const TimePoint now = Clock::now();
    if (now - last_launch_time_ < kMinLaunchInterval)
    {
        LOG(WARNING) << "Too many launch requests (script:" << script.name << ")";
        return;
    }

    last_launch_time_ = now;

    if (script_id_ != -1)
    {
        LOG(WARNING) << "Script" << script.name << "refused, script" << script_id_ << "is still running";
        emit sig_message(client_id, scriptResult(id, proto::tools::ScriptResult::STATUS_BUSY));
        return;
    }

    if (!hasInteractiveUser(session_id))
    {
        LOG(ERROR) << "No interactive user in session" << session_id;
        emit sig_message(client_id, scriptResult(id, proto::tools::ScriptResult::STATUS_NOT_STARTED));
        return;
    }

    script_id_ = id;
    script_client_id_ = client_id;

    if (!launchScript(session_id, script, QSize(execute_script.columns(), execute_script.rows())))
    {
        LOG(ERROR) << "Unable to start script" << script.name << "in session" << session_id;
        sendScriptResult(proto::tools::ScriptResult::STATUS_NOT_STARTED);
    }
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::terminateScript()
{
    LOG(INFO) << "Stopping script" << script_id_;

    script_process_->disconnect(this);
    script_process_.reset();
    sendScriptResult(proto::tools::ScriptResult::STATUS_FINISHED);
}

//--------------------------------------------------------------------------------------------------
void ToolsWorker::sendScriptResult(proto::tools::ScriptResult::Status status)
{
    LOG(INFO) << "Script" << script_id_ << "ended (status:" << status << ")";

    emit sig_message(script_client_id_, scriptResult(script_id_, status));
    script_id_ = -1;
}

//--------------------------------------------------------------------------------------------------
// static
QString ToolsWorker::scriptText(const Script& script)
{
    QFile file(QString(":/scripts/") + script.file);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open script" << script.file << ":" << file.errorString();
        return QString();
    }

    // The file is stored with the line endings of the repository, and a shell reading it wants its own.
    return QString::fromUtf8(file.readAll()).replace("\r\n", "\n");
}

//--------------------------------------------------------------------------------------------------
bool ToolsWorker::launchScript(SessionId session_id, const Script& script, const QSize& size)
{
    ScopedQPointer<TerminalProcess> process(createScriptProcess(session_id, script));
    if (!process)
        return false;

    connect(process.get(), &TerminalProcess::sig_output, this, &ToolsWorker::onScriptOutput);
    connect(process.get(), &TerminalProcess::sig_finished, this, [this]()
    {
        script_process_.reset();
        sendScriptResult(proto::tools::ScriptResult::STATUS_FINISHED);
    });

    const int columns = std::clamp(size.width(), 40, 500);
    const int rows = std::clamp(size.height(), 10, 200);

    LOG(INFO) << "Starting script" << script.name << "in session" << session_id
              << "(size:" << columns << "x" << rows << ")";

    if (!process->start(columns, rows))
        return false;

    script_process_ = std::move(process);
    onScriptOutput(QByteArray());
    return true;
}
