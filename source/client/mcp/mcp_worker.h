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

#ifndef CLIENT_MCP_MCP_WORKER_H
#define CLIENT_MCP_MCP_WORKER_H

#include <QByteArray>
#include <QHash>

#include <vector>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"

class McpTool;
class QLocalServer;
class QLocalSocket;
class RouterController;

class McpWorker final : public Worker
{
    Q_OBJECT

public:
    McpWorker();
    ~McpWorker() final;

    // Runs the process that an AI application launches with --mcp. The process passes the messages
    // between its standard streams and the running client. Returns the exit code of the process.
    static int runBridge();

public slots:
    void onEnable(const QPointer<RouterController>& router_controller);
    void onDisable();

signals:
    void sig_enableFailed();

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;

private slots:
    void onNewConnection();

private:
    struct Connection
    {
        QByteArray buffer;
        bool initialized = false;
    };

    void readMessages(QLocalSocket* socket);

    ScopedQPointer<QLocalServer> server_;
    QHash<QLocalSocket*, Connection> connections_;

    QPointer<RouterController> router_controller_;
    std::vector<std::unique_ptr<McpTool>> tools_;

    Q_DISABLE_COPY_MOVE(McpWorker)
};

#endif // CLIENT_MCP_MCP_WORKER_H
