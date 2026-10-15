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

#ifndef CLIENT_DESKTOP_MCP_CONNECTOR_H
#define CLIENT_DESKTOP_MCP_CONNECTOR_H

#include <QByteArray>
#include <QHash>
#include <QObject>

class QLocalServer;
class QLocalSocket;

// MCP server of the running client. An AI application launches the client with --mcp, and that
// process passes the messages between its standard streams and the running client.
class McpConnector final : public QObject
{
    Q_OBJECT

public:
    explicit McpConnector(QObject* parent = nullptr);
    ~McpConnector() final;

    // Runs the process launched by the AI application. Returns the exit code of the process.
    static int runBridge();

    bool start();
    void stop();

private slots:
    void onNewConnection();

private:
    struct Connection
    {
        QByteArray buffer;
        bool initialized = false;
    };

    void readMessages(QLocalSocket* socket);

    QLocalServer* server_ = nullptr;
    QHash<QLocalSocket*, Connection> connections_;

    Q_DISABLE_COPY_MOVE(McpConnector)
};

#endif // CLIENT_DESKTOP_MCP_CONNECTOR_H
