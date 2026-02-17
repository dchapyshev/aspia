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

#ifndef CLIENT_UI_DESKTOP_TASK_MANAGER_WINDOW_H
#define CLIENT_UI_DESKTOP_TASK_MANAGER_WINDOW_H

#include <QTreeWidget>

#include "proto/task_manager.h"
#include "ui_task_manager_window.h"

class QHBoxLayout;
class QLabel;
class QStatusBar;

namespace client {

class TaskManagerWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit TaskManagerWindow(QWidget* parent = nullptr);
    ~TaskManagerWindow() final;

    void readMessage(const proto::task_manager::HostToClient& message);

signals:
    void sig_sendMessage(const proto::task_manager::ClientToHost& request);

protected:
    // QMainWindow implementation.
    void closeEvent(QCloseEvent* event) final;

private slots:
    void onProcessHeaderContextMenu(const QPoint& pos);
    void onServiceHeaderContextMenu(const QPoint& pos);
    void onUserHeaderContextMenu(const QPoint& pos);
    void onProcessContextMenu(const QPoint& pos);
    void onServiceContextMenu(const QPoint& pos);
    void onUserContextMenu(const QPoint& pos);
    void onEndProcess();
    void onStartService();
    void onStopService();
    void onDisconnectUser();
    void onLogoffUser();

private:
    void sendProcessListRequest(quint32 flags);
    void sendEndProcessRequest(quint64 process_id);
    void sendServiceListRequest();
    void sendServiceRequest(const std::string& name, proto::task_manager::ServiceRequest::Command command);
    void sendUserListRequest();
    void sendUserRequest(quint32 session_id, proto::task_manager::UserRequest::Command command);
    void readProcessList(const proto::task_manager::ProcessList& process_list);
    void readServiceList(const proto::task_manager::ServiceList& service_list);
    void readUserList(const proto::task_manager::UserList& user_list);

    void setProcessCount(int count);
    void setCpuUsage(int usage);
    void setMemoryUsage(int usage);
    void addUpdateItems(QMenu* parent_menu);

    Ui::TaskManagerWindow ui;
    QLabel* label_process_ = nullptr;
    QLabel* label_cpu_ = nullptr;
    QLabel* label_memory_ = nullptr;

    QTimer* update_timer_ = nullptr;

    Q_DISABLE_COPY(TaskManagerWindow)
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_TASK_MANAGER_WINDOW_H
