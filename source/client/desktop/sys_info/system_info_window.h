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

#ifndef CLIENT_DESKTOP_SYS_INFO_SYSTEM_INFO_WINDOW_H
#define CLIENT_DESKTOP_SYS_INFO_SYSTEM_INFO_WINDOW_H

#include "client/desktop/client_window.h"
#include "client/mcp/mcp_controller.h"

namespace proto::system_info {
class SystemInfoRequest;
} // namespace proto::system_info

class SysInfoView;

class SystemInfoWindow final
    : public ClientWindow,
      public McpController::SystemInfoWindow
{
    Q_OBJECT

public:
    explicit SystemInfoWindow(QWidget* parent = nullptr);
    ~SystemInfoWindow() final;

    // ClientWindow implementation.
    void setTabbedMode(bool tabbed) final;
    QList<QPair<Tab::ActionRole, QList<QAction*>>> tabActionGroups() const final;
    QByteArray saveState() const final;
    void restoreState(const QByteArray& state) final;

protected:
    // ClientWindow implementation.
    void onInternalReset() final;
    void onRegisterWorkers() final;
    void onSessionStarted() final;

    // McpController::SystemInfoWindow implementation.
    QList<SysInfoView::Page> mcpPages() const final;
    bool mcpShowPage(const QString& key) final;
    void mcpBuildReport(SysInfoReport* report) final;
    void mcpRefresh() final;
    proto::system_info::EventLogs::Event::Type mcpCurrentEventLog() const final;
    bool mcpCanMoveEventLog(SysInfoWidgetEventLogs::Move to) const final;
    bool mcpSelectEventLog(proto::system_info::EventLogs::Event::Type type) final;
    bool mcpMoveEventLog(SysInfoWidgetEventLogs::Move to) final;

private slots:
    void onChannelMessage(const QByteArray& buffer);
    void onSystemInfoRequest(const proto::system_info::SystemInfoRequest& request);

private:
    SysInfoView* view_ = nullptr;
    SysInfoWidgetEventLogs* event_logs_ = nullptr;

    Q_DISABLE_COPY_MOVE(SystemInfoWindow)
};

#endif // CLIENT_DESKTOP_SYS_INFO_SYSTEM_INFO_WINDOW_H
