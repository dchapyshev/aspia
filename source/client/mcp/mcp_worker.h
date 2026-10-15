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
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"

class QLocalServer;
class QLocalSocket;

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
    void onEnable();
    void onDisable();
    void onToolsListed(qint64 request_id, const QJsonArray& tools);
    void onToolFinished(qint64 request_id, bool success, const QString& text);
    void onToolNotFound(qint64 request_id, const QString& name);

signals:
    void sig_enableFailed();
    void sig_toolsListRequested(qint64 request_id);
    void sig_toolCallRequested(qint64 request_id, const QString& name, const QJsonObject& arguments);

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;

private slots:
    void onNewConnection();

private:
    using Respond = std::function<void(const QJsonObject& response)>;

    struct Connection
    {
        QByteArray buffer;
        bool initialized = false;
    };

    // A request that waits for the answer of the tools.
    struct Pending
    {
        QJsonValue id;
        bool modern = false;
        Respond respond;
    };

    void readMessages(QLocalSocket* socket);
    void processMessage(const QByteArray& message, Connection* connection, const Respond& respond);
    void processMethod(const QJsonValue& id, const QString& method, const QJsonObject& params, bool modern,
                       const Respond& respond);

    ScopedQPointer<QLocalServer> server_;
    QHash<QLocalSocket*, Connection> connections_;

    QHash<qint64, Pending> pending_;
    qint64 last_request_id_ = 0;

    Q_DISABLE_COPY_MOVE(McpWorker)
};

#endif // CLIENT_MCP_MCP_WORKER_H
