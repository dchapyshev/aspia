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
#include <QList>
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
    struct SessionInfo
    {
        proto::peer::SessionType session_type;
        QString name;
        qint64 entry_id = 0;
        qint64 router_id = 0;
        QString address;
        bool portable = false;
        bool detached = false;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual QList<SessionInfo> sessions() const = 0;
        virtual void connectLocalHost(qint64 entry_id, proto::peer::SessionType session_type) = 0;
        virtual void connectRouterHost(const HostConfig& host, proto::peer::SessionType session_type) = 0;
    };

    // Only one lives at a time. |delegate| must outlive the controller.
    McpController(Delegate* delegate, QObject* parent = nullptr);
    ~McpController() final;

    static McpController& instance();

    // A failure to turn the connector on comes as sig_enableFailed().
    void setEnabled(bool enable);

    QList<SessionInfo> sessions() const;

    // The session starts when the event loop gets control, so a modal dialog of the session does not
    // hold the reply of the tool.
    void connectLocalHost(qint64 entry_id, proto::peer::SessionType session_type);
    void connectRouterHost(const HostConfig& host, proto::peer::SessionType session_type);

signals:
    void sig_enableFailed();

private slots:
    void onToolsListRequested(qint64 request_id);
    void onToolCallRequested(qint64 request_id, const QString& name, const QJsonObject& arguments);

private:
    Delegate* delegate_;
    QPointer<McpWorker> mcp_worker_;
    std::vector<std::unique_ptr<McpTool>> tools_;

    Q_DISABLE_COPY_MOVE(McpController)
};

#endif // CLIENT_MCP_MCP_CONTROLLER_H
