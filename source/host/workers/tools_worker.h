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

#ifndef HOST_WORKERS_TOOLS_WORKER_H
#define HOST_WORKERS_TOOLS_WORKER_H

#include <QByteArray>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>

#include <span>

#include "base/scoped_qpointer.h"
#include "base/session_id.h"
#include "base/threading/worker.h"
#include "proto/desktop_tools.h"

class TerminalProcess;

class ToolsWorker final : public Worker
{
    Q_OBJECT

public:
    ToolsWorker();
    ~ToolsWorker() final;

    // Parses the serialized ClientToHost in |buffer| received from |client_id| and processes it in the
    // worker thread. |session_id| is the session the client currently sees. May be called from any
    // thread.
    void query(quint32 client_id, SessionId session_id, const QByteArray& buffer);

    // Builds the tool list for |session_id| without a request from the client: the host pushes the
    // list as soon as the session becomes available. Same threading rules as query().
    void requestToolList(quint32 client_id, SessionId session_id);

    // Stops the script started by |client_id| if it is still running. May be called from any thread.
    void stopScript(quint32 client_id);

signals:
    // Serialized HostToClient for |client_id|.
    void sig_message(quint32 client_id, const QByteArray& buffer);

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;

private slots:
    void onScriptOutput(const QByteArray& data);

private:
    // One management tool available on this host. Only the name and the icon are sent to the client:
    // how the tool is started is known to the host alone. The identifier the client sends back is
    // the position of the tool in |tools_|.
    struct Tool
    {
        QString name;
        QByteArray icon; // PNG. May be empty.

        QString program;
        QStringList arguments;

        // The tool is started with the unfiltered token of the logged on user: an administrator gets
        // the rights UAC would otherwise ask them to confirm.
        bool elevated = false;
    };

    // One maintenance script. The client is told only what to put in the menu and receives what the
    // script prints, the script itself never leaves the host. The identifier the client sends back is
    // the position of the script in scriptTable().
    struct Script
    {
        const char* name;
        const char* description;

        // Name of the file the body of the script is kept in, under ":/scripts".
        const char* file;

        // The client asks the operator to confirm before starting it.
        bool confirm;
    };

    void readMessage(quint32 client_id, SessionId session_id, const proto::tools::ClientToHost& message);
    QByteArray toolList(SessionId session_id);
    void executeTool(SessionId session_id, qint32 id);
    void executeScript(quint32 client_id, SessionId session_id, const proto::tools::ExecuteScript& execute_script);
    void terminateScript();
    void sendScriptResult(proto::tools::ScriptResult::Status status);

    // Fills |tools_| with the tools found on this machine. The result is defined by the composition
    // of the operating system, which does not change while the service is running, so it is built
    // once.
    void buildToolList();

    // Returns the scripts written for this operating system. The table is a constant of the program,
    // unlike the tools, which are whatever the machine happens to have installed.
    static std::span<const Script> scriptTable();

    // Returns true if this machine has what the scripts of the table need to run.
    bool scriptsSupported() const;

    // Returns the body of |script|, read from the resources of the host. Empty on failure.
    static QString scriptText(const Script& script);

    // Returns true if |session_id| is an interactive session with a logged on user. Tools are
    // offered to the client only in that case: there is no desktop to show them on otherwise.
    bool hasInteractiveUser(SessionId session_id) const;

    // Starts |tool| in |session_id|. The service itself lives in a non-interactive session, so the
    // process has to be created with a token of the target session.
    bool launchTool(SessionId session_id, const Tool& tool) const;

    // Starts |script| for |session_id| in a terminal of |size|. Unlike a tool, a script does what the
    // logged on user is usually not allowed to do, so it runs with the privileges of the service.
    bool launchScript(SessionId session_id, const Script& script, const QSize& size);

    TerminalProcess* createScriptProcess(SessionId session_id, const Script& script) const;

    QList<Tool> tools_;

    // Whether the scripts of the table can run on this machine, asked once at the start.
    bool scripts_supported_ = false;

    qint32 script_id_ = -1;
    quint32 script_client_id_ = 0;
    ScopedQPointer<TerminalProcess> script_process_;

    // Time of the last launch. Without it a client is able to fill the user session with processes
    // as fast as it can send messages.
    TimePoint last_launch_time_;

    // Time of the last list sent at the request of the client.
    TimePoint last_list_time_;

    Q_DISABLE_COPY_MOVE(ToolsWorker)
};

#endif // HOST_WORKERS_TOOLS_WORKER_H
