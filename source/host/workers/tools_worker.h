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
#include <QString>
#include <QStringList>

#include <functional>

#include "base/session_id.h"
#include "base/threading/worker.h"

namespace proto::tools {
class ClientToHost;
} // namespace proto::tools

class ToolsWorker final : public Worker
{
    Q_OBJECT

public:
    ToolsWorker();
    ~ToolsWorker() final;

    // Parses the serialized ClientToHost in |buffer|, processes it in the worker thread and
    // delivers the serialized reply (empty when the request produces no response) to |reply| in the
    // calling worker's thread. |session_id| is the session the client currently sees. The reply is
    // dropped if |context| is destroyed before it is ready. May be called from any worker thread.
    void query(QObject* context, SessionId session_id, const QByteArray& buffer,
               std::function<void(QByteArray)> reply);

    // Builds the tool list for |session_id| without a request from the client: the host pushes the
    // list as soon as the session becomes available. Same threading rules as query().
    void requestToolList(QObject* context, SessionId session_id,
                         std::function<void(QByteArray)> reply);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

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

    // One maintenance script. Its window is shown on the desktop of the user, so the client is told
    // only what to put in the menu; the script itself never leaves the host. The identifier the
    // client sends back is the position of the script in |scripts_|.
    struct Script
    {
        QString name;
        QString description;

        // The body of the script in the language of the shell of the system.
        QString command;

        // The client asks the operator to confirm before starting it.
        bool confirm = false;

        // The script is started with the token of the logged on user instead of the token of the
        // service. Needed by the few scripts that act on behalf of that user.
        bool as_user = false;
    };

    QByteArray readMessage(SessionId session_id, const proto::tools::ClientToHost& message);
    QByteArray toolList(SessionId session_id);
    QByteArray executeTool(SessionId session_id, qint32 id);
    QByteArray executeScript(SessionId session_id, qint32 id);

    // Fills |tools_| with the tools found on this machine. The result is defined by the composition
    // of the operating system, which does not change while the service is running, so it is built
    // once.
    void buildToolList();

    // Fills |scripts_| with the scripts this operating system is able to run.
    void buildScriptList();

    // Returns true if |session_id| is an interactive session with a logged on user. Tools are
    // offered to the client only in that case: there is no desktop to show them on otherwise.
    bool hasInteractiveUser(SessionId session_id) const;

    // Starts |tool| in |session_id|. The service itself lives in a non-interactive session, so the
    // process has to be created with a token of the target session.
    bool launchTool(SessionId session_id, const Tool& tool) const;

    // Starts |script| in |session_id| in a console window of its own, so that the person controlling
    // the desktop sees it running. Unlike a tool, a script does what the logged on user is usually
    // not allowed to do, so it runs with the privileges of the service.
    bool launchScript(SessionId session_id, const Script& script) const;

    QList<Tool> tools_;
    QList<Script> scripts_;

    // Time of the last launch. Without it a client is able to fill the user session with processes
    // as fast as it can send messages.
    TimePoint last_launch_time_;

    Q_DISABLE_COPY_MOVE(ToolsWorker)
};

#endif // HOST_WORKERS_TOOLS_WORKER_H
