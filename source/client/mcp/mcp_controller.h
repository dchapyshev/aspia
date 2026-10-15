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

#ifndef CLIENT_MCP_MCP_CONTROLLER_H
#define CLIENT_MCP_MCP_CONTROLLER_H

#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class HostConfig;
class McpTool;
class McpWorker;

// The tools of the MCP connector. They run in the GUI thread, and the worker keeps only the
// connections and the protocol. The connector is turned off when the controller dies.
class McpController final : public QObject
{
    Q_OBJECT

public:
    // Only one lives at a time.
    explicit McpController(QObject* parent = nullptr);
    ~McpController() final;

    static McpController& instance();

    // A failure to turn the connector on comes as sig_enableFailed().
    void setEnabled(bool enable);

signals:
    void sig_enableFailed();

    void sig_localConnectRequested(qint64 entry_id, proto::peer::SessionType session_type);
    void sig_routerConnectRequested(const HostConfig& host, proto::peer::SessionType session_type);

private slots:
    void onToolsListRequested(qint64 request_id);
    void onToolCallRequested(qint64 request_id, const QString& name, const QJsonObject& arguments);

private:
    QPointer<McpWorker> mcp_worker_;
    std::vector<std::unique_ptr<McpTool>> tools_;

    Q_DISABLE_COPY_MOVE(McpController)
};

#endif // CLIENT_MCP_MCP_CONTROLLER_H
