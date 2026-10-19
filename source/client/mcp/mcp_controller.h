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
#include <QStringList>

#include <memory>
#include <vector>

#include "base/time_types.h"
#include "client/file_transfer.h"
#include "common/file_task.h"
#include "common/desktop/chat_widget.h"
#include "common/sys_info/sys_info_view.h"
#include "common/sys_info/sys_info_widget_event_logs.h"

namespace proto::input {
class MouseEvent;
} // namespace proto::input

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

namespace proto::screen {
class Screen;
class ScreenList;
} // namespace proto::screen

class HostConfig;
class McpTool;
class McpWorker;
class QImage;
class QPoint;
class QSize;
class SysInfoReport;

// The tools of the MCP connector. They run in the GUI thread, and the worker keeps only the
// connections and the protocol. The connector is turned off when the controller dies.
class McpController final : public QObject
{
    Q_OBJECT

public:
    enum class SignInResult { STARTED, NOT_REQUESTED, NOT_SELECTED };
    Q_ENUM(SignInResult)

    enum class ClientState { CONNECTING, CONNECTED, DISCONNECTED, AUTHORIZATION, DIALOG };
    Q_ENUM(ClientState)

    struct ClientInfo
    {
        qint64 client_id = 0;
        proto::peer::SessionType session_type;
        QString name;
        qint64 entry_id = 0;
        qint64 router_id = 0;
        QString address;
        bool portable = false;
        bool detached = false;
        bool current = false;
        ClientState state = ClientState::CONNECTING;
        QString message;
    };

    class DesktopWindow
    {
    public:
        virtual ~DesktopWindow() = default;

        virtual QSize mcpFrameSize() const = 0;
        virtual QImage mcpScreenshot() const = 0;
        virtual void mcpSendMouseEvent(const proto::input::MouseEvent& event) = 0;
        virtual void mcpSendKeyEvent(quint32 usb_keycode, bool pressed) = 0;
        virtual void mcpSendText(const QString& text) = 0;
        virtual proto::screen::ScreenList mcpScreenList() const = 0;
        virtual void mcpSelectScreen(const proto::screen::Screen& screen) = 0;
        virtual SignInResult mcpSignIn() = 0;
    };

    class FileTransferWindow
    {
    public:
        struct TaskState
        {
            enum class Task { NONE, TRANSFER, REMOVAL };

            Task task = Task::NONE;
            int total_progress = 0;
            int current_progress = 0;
            QString dialog;
        };

        virtual ~FileTransferWindow() = default;

        virtual QString mcpCurrentPath(FileTask::Target target) const = 0;
        virtual bool mcpOpenPath(FileTask::Target target, const QString& path) = 0;
        virtual bool mcpIsCurrentPathListed(FileTask::Target target) const = 0;
        virtual QList<FileTransfer::Item> mcpCurrentItems(FileTask::Target target) const = 0;
        virtual bool mcpIsBusy() const = 0;
        virtual QStringList mcpSelectItems(FileTask::Target target, const QStringList& names) = 0;
        virtual bool mcpSendSelection(FileTask::Target target, bool replace_existing) = 0;
        virtual bool mcpRemoveSelection(FileTask::Target target, bool confirmed) = 0;
        virtual bool mcpCreateFolder(FileTask::Target target, const QString& name) = 0;
        virtual bool mcpRenameItem(FileTask::Target target, const QString& old_name, const QString& new_name) = 0;
        virtual TaskState mcpTaskState() const = 0;
        virtual bool mcpCloseDialog(bool abort) = 0;
        virtual bool mcpCancelTask() = 0;
    };

    class TerminalWindow
    {
    public:
        virtual ~TerminalWindow() = default;

        virtual SignInResult mcpSignIn() = 0;
        virtual bool mcpHasLoginPrompt() const = 0;
        virtual void mcpSendText(const QString& text) = 0;
        virtual void mcpSendKey(int key, Qt::KeyboardModifiers modifiers) = 0;
        virtual QSize mcpGridSize() const = 0;
        virtual QPoint mcpCursorPosition() const = 0;
        virtual QStringList mcpLines(int history) const = 0;
        virtual QImage mcpScreenshot() const = 0;
        virtual TimePoint mcpLastInputTime() const = 0;
        virtual TimePoint mcpLastOutputTime() const = 0;
    };

    class ChatWindow
    {
    public:
        virtual ~ChatWindow() = default;

        virtual QList<ChatWidget::Message> mcpMessages() const = 0;
        virtual QString mcpChatStatus() const = 0;
        virtual bool mcpSendText(const QString& text) = 0;
    };

    class SystemInfoWindow
    {
    public:
        virtual ~SystemInfoWindow() = default;

        virtual QList<SysInfoView::Page> mcpPages() const = 0;
        virtual bool mcpShowPage(const QString& key) = 0;
        virtual void mcpBuildReport(SysInfoReport* report) = 0;
        virtual void mcpRefresh() = 0;
        virtual proto::system_info::EventLogs::Event::Type mcpCurrentEventLog() const = 0;
        virtual bool mcpCanMoveEventLog(SysInfoWidgetEventLogs::Move to) const = 0;
        virtual bool mcpSelectEventLog(proto::system_info::EventLogs::Event::Type type) = 0;
        virtual bool mcpMoveEventLog(SysInfoWidgetEventLogs::Move to) = 0;
    };

    class MainWindow
    {
    public:
        virtual ~MainWindow() = default;

        virtual QList<ClientInfo> mcpClients() const = 0;
        virtual DesktopWindow* mcpDesktopWindow(qint64 client_id) const = 0;
        virtual FileTransferWindow* mcpFileTransferWindow(qint64 client_id) const = 0;
        virtual TerminalWindow* mcpTerminalWindow(qint64 client_id) const = 0;
        virtual SystemInfoWindow* mcpSystemInfoWindow(qint64 client_id) const = 0;
        virtual ChatWindow* mcpChatWindow(qint64 client_id) const = 0;
        virtual QString mcpClientDialog(qint64 client_id) const = 0;
        virtual void mcpCloseClient(qint64 client_id) = 0;
        virtual bool mcpShowClient(qint64 client_id) = 0;
        virtual bool mcpAsksCredentials() const = 0;
        virtual void mcpConnectLocalHost(qint64 entry_id, proto::peer::SessionType session_type) = 0;
        virtual void mcpConnectRouterHost(const HostConfig& host, proto::peer::SessionType session_type) = 0;
    };

    // Only one lives at a time. |main_window| must outlive the controller.
    McpController(MainWindow* main_window, QObject* parent = nullptr);
    ~McpController() final;

    static McpController& instance();

    // A failure to turn the connector on comes as sig_enableFailed().
    void setEnabled(bool enable);

    QList<ClientInfo> clients() const;
    DesktopWindow* desktopWindow(qint64 client_id) const;
    FileTransferWindow* fileTransferWindow(qint64 client_id) const;
    TerminalWindow* terminalWindow(qint64 client_id) const;
    SystemInfoWindow* systemInfoWindow(qint64 client_id) const;
    ChatWindow* chatWindow(qint64 client_id) const;
    QString clientDialog(qint64 client_id) const;
    void closeClient(qint64 client_id);
    bool showClient(qint64 client_id);

    // True while a window that is not opened yet asks the user for the credentials of the host.
    bool asksCredentials() const;

    // The session starts when the event loop gets control, so a modal dialog of the session does not
    // hold the reply of the tool.
    void connectLocalHost(qint64 entry_id, proto::peer::SessionType session_type);
    void connectRouterHost(const HostConfig& host, proto::peer::SessionType session_type);

signals:
    void sig_enableFailed();

    // The AI applications that are connected now. The name is empty when an application does not give it.
    void sig_clientsChanged(const QStringList& clients);

private slots:
    void onToolsListRequested(qint64 request_id);
    void onToolCallRequested(qint64 request_id, const QString& name, const QJsonObject& arguments);

private:
    MainWindow* main_window_ = nullptr;
    QPointer<McpWorker> mcp_worker_;
    std::vector<std::unique_ptr<McpTool>> tools_;

    Q_DISABLE_COPY_MOVE(McpController)
};

#endif // CLIENT_MCP_MCP_CONTROLLER_H
