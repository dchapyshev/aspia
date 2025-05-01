//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/desktop/task_manager_window.h"

#include "base/logging.h"
#include "client/ui/desktop/task_manager_settings.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

namespace client {

namespace {

enum ProcessColumn
{
    PROC_COL_NAME                    = 0,
    PROC_COL_PROCESS_ID              = 1,
    PROC_COL_SESSION_ID              = 2,
    PROC_COL_USER_NAME               = 3,
    PROC_COL_CPU_USAGE               = 4,
    PROC_COL_MEM_PRIVATE_WORKING_SET = 5,
    PROC_COL_MEM_WORKING_SET         = 6,
    PROC_COL_MEM_PEAK_WORKING_SET    = 7,
    PROC_COL_MEM_WORKING_SET_DELTA   = 8,
    PROC_COL_THREAD_COUNT            = 9,
    PROC_COL_IMAGE_PATH              = 10
};

enum ServiceColumn
{
    SERV_COL_NAME         = 0,
    SERV_COL_STATE        = 1,
    SERV_COL_STARTUP_TYPE = 2,
    SERV_COL_DESCRIPTION  = 3
};

enum UserColumn
{
    USER_COL_NAME         = 0,
    USER_COL_ID           = 1,
    USER_COL_STATE        = 2,
    USER_COL_CLIENT_NAME  = 3,
    USER_COL_SESSION_NAME = 4
};

class ProcessItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(ProcessItem)

public:
    explicit ProcessItem(const proto::task_manager::Process& process)
    {
        setIcon(PROC_COL_NAME, QIcon(QStringLiteral(":/img/application.png")));

        setTextAlignment(PROC_COL_MEM_PRIVATE_WORKING_SET, Qt::AlignRight);
        setTextAlignment(PROC_COL_MEM_WORKING_SET, Qt::AlignRight);
        setTextAlignment(PROC_COL_MEM_PEAK_WORKING_SET, Qt::AlignRight);
        setTextAlignment(PROC_COL_MEM_WORKING_SET_DELTA, Qt::AlignRight);
        setTextAlignment(PROC_COL_THREAD_COUNT, Qt::AlignRight);

        updateItem(process);

        QString process_name;
        if (process.process_name().empty())
        {
            if (process_.process_id() == 0)
                process_name = tr("System Idle Process");
            else
                process_name = tr("Unknown Process");
        }
        else
        {
            process_name = QString::fromStdString(process.process_name());
        }

        QString user_name;
        if (process.process_id() == 0)
            user_name = QStringLiteral("SYSTEM");
        else
            user_name = QString::fromStdString(process.user_name());

        setText(PROC_COL_NAME, process_name);
        setText(PROC_COL_USER_NAME, user_name);
        setText(PROC_COL_IMAGE_PATH, QString::fromStdString(process.file_path()));
        setText(PROC_COL_PROCESS_ID, QString::number(process.process_id()));
        setText(PROC_COL_SESSION_ID, QString::number(process.session_id()));
    }

    quint64 processId() const { return process_.process_id(); }

    void updateItem(const proto::task_manager::Process& process)
    {
        process_ = process;

        setText(PROC_COL_CPU_USAGE, QString::asprintf("%02d", process.cpu_usage()));
        setText(PROC_COL_MEM_PRIVATE_WORKING_SET, sizeToString(process_.mem_private_working_set()));
        setText(PROC_COL_MEM_WORKING_SET, sizeToString(process_.mem_working_set()));
        setText(PROC_COL_MEM_PEAK_WORKING_SET, sizeToString(process_.mem_peak_working_set()));
        setText(PROC_COL_MEM_WORKING_SET_DELTA, sizeToString(process_.mem_working_set_delta()));
        setText(PROC_COL_THREAD_COUNT, QString::number(process_.thread_count()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        const ProcessItem* other_item = static_cast<const ProcessItem*>(&other);

        switch (treeWidget()->sortColumn())
        {
            case PROC_COL_PROCESS_ID:
                return process_.process_id() < other_item->process_.process_id();

            case PROC_COL_SESSION_ID:
                return process_.session_id() < other_item->process_.session_id();

            case PROC_COL_CPU_USAGE:
                return process_.cpu_usage() < other_item->process_.cpu_usage();

            case PROC_COL_MEM_PRIVATE_WORKING_SET:
                return process_.mem_private_working_set() < other_item->process_.mem_private_working_set();

            case PROC_COL_MEM_WORKING_SET:
                return process_.mem_working_set() < other_item->process_.mem_working_set();

            case PROC_COL_MEM_PEAK_WORKING_SET:
                return process_.mem_peak_working_set() < other_item->process_.mem_peak_working_set();

            case PROC_COL_MEM_WORKING_SET_DELTA:
                return process_.mem_working_set_delta() < other_item->process_.mem_working_set_delta();

            case PROC_COL_THREAD_COUNT:
                return process_.thread_count() < other_item->process_.thread_count();

            default:
                break;
        }

        return QTreeWidgetItem::operator<(other);
    }

    static QString sizeToString(int64_t size)
    {
        return QStringLiteral("%1 K").arg(size / 1024LL);
    }

private:
    proto::task_manager::Process process_;
};

class ServiceItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(ServiceItem)

public:
    explicit ServiceItem(const proto::task_manager::Service& service)
        : name_(service.name()),
          status_(service.status()),
          startup_type_(service.startup_type())
    {
        setText(SERV_COL_NAME, QString::fromStdString(service.display_name()));
        setText(SERV_COL_STATE, statusToString(service.status()));
        setText(SERV_COL_STARTUP_TYPE, startupTypeToString(service.startup_type()));
        setText(SERV_COL_DESCRIPTION, QString::fromStdString(service.description()));
    }

    std::string name() const { return name_; }
    proto::task_manager::Service::Status status() const { return status_; }
    proto::task_manager::Service::StartupType startupType() const { return startup_type_; }

private:
    // static
    QString statusToString(proto::task_manager::Service::Status status)
    {
        switch (status)
        {
            case proto::task_manager::Service::STATUS_CONTINUE_PENDING:
                return tr("Continue Pending");

            case proto::task_manager::Service::STATUS_PAUSE_PENDING:
                return tr("Pause Pending");

            case proto::task_manager::Service::STATUS_PAUSED:
                return tr("Paused");

            case proto::task_manager::Service::STATUS_RUNNING:
                return tr("Running");

            case proto::task_manager::Service::STATUS_START_PENDING:
                return tr("Start Pending");

            case proto::task_manager::Service::STATUS_STOP_PENDING:
                return tr("Stop Pending");

            case proto::task_manager::Service::STATUS_STOPPED:
                return tr("Stopped");

            default:
                return tr("Unknown");
        }
    }

    // static
    QString startupTypeToString(proto::task_manager::Service::StartupType startup_type)
    {
        switch (startup_type)
        {
            case proto::task_manager::Service::STARTUP_TYPE_AUTO_START:
                return tr("Auto Start");

            case proto::task_manager::Service::STARTUP_TYPE_DEMAND_START:
                return tr("Demand Start");

            case proto::task_manager::Service::STARTUP_TYPE_DISABLED:
                return tr("Disabled");

            case proto::task_manager::Service::STARTUP_TYPE_BOOT_START:
                return tr("Boot Start");

            case proto::task_manager::Service::STARTUP_TYPE_SYSTEM_START:
                return tr("System Start");

            default:
                return tr("Unknown");
        }
    }

    const std::string name_;
    const proto::task_manager::Service::Status status_;
    const proto::task_manager::Service::StartupType startup_type_;
};

class UserItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(UserItem)

public:
    explicit UserItem(const proto::task_manager::User& user)
        : session_id_(user.session_id())
    {
        setIcon(USER_COL_NAME, QIcon(QStringLiteral(":/img/user.png")));
        setText(USER_COL_ID, QString::number(user.session_id()));

        updateItem(user);
    }

    void updateItem(const proto::task_manager::User& user)
    {
        QString user_name;

        if (user.user_name().empty())
            user_name = tr("<no user>");
        else
            user_name = QString::fromStdString(user.user_name());

        setText(USER_COL_NAME, user_name);
        setText(USER_COL_STATE, connectStateToString(user.connect_state()));
        setText(USER_COL_CLIENT_NAME, QString::fromStdString(user.client_name()));
        setText(USER_COL_SESSION_NAME, QString::fromStdString(user.session_name()));
    }

    uint32_t sessionId() const { return session_id_; }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        const UserItem* other_item = static_cast<const UserItem*>(&other);

        switch (treeWidget()->sortColumn())
        {
            case USER_COL_ID:
                return session_id_ < other_item->session_id_;

            default:
                break;
        }

        return QTreeWidgetItem::operator<(other);
    }

private:
    static QString connectStateToString(proto::task_manager::User::ConnectState connect_state)
    {
        switch (connect_state)
        {
            case proto::task_manager::User::CONNECT_STATE_ACTIVE:
                return tr("Active");

            case proto::task_manager::User::CONNECT_STATE_CONNECTED:
                return tr("Connected");

            case proto::task_manager::User::CONNECT_STATE_CONNECT_QUERY:
                return tr("Connect Query");

            case proto::task_manager::User::CONNECT_STATE_SHADOW:
                return tr("Shadow");

            case proto::task_manager::User::CONNECT_STATE_DISCONNECTED:
                return tr("Disconnected");

            case proto::task_manager::User::CONNECT_STATE_IDLE:
                return tr("Idle");

            case proto::task_manager::User::CONNECT_STATE_LISTEN:
                return tr("Listen");

            case proto::task_manager::User::CONNECT_STATE_RESET:
                return tr("Reset");

            case proto::task_manager::User::CONNECT_STATE_DOWN:
                return tr("Down");

            case proto::task_manager::User::CONNECT_STATE_INIT:
                return tr("Init");

            default:
                return tr("Unknown");
        }
    }

    const uint32_t session_id_;
};

class ColumnAction final : public QAction
{
public:
    ColumnAction(const QString& text, int index, QObject* parent)
        : QAction(text, parent),
          index_(index)
    {
        setCheckable(true);
    }

    int columnIndex() const { return index_; }

private:
    const int index_;
    DISALLOW_COPY_AND_ASSIGN(ColumnAction);
};

} // namespace

//--------------------------------------------------------------------------------------------------
TaskManagerWindow::TaskManagerWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    ui.tree_processes->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.tree_services->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.tree_users->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.tree_processes->header(), &QHeaderView::customContextMenuRequested,
            this, &TaskManagerWindow::onProcessHeaderContextMenu);
    connect(ui.tree_services->header(), &QHeaderView::customContextMenuRequested,
            this, &TaskManagerWindow::onServiceHeaderContextMenu);
    connect(ui.tree_users->header(), &QHeaderView::customContextMenuRequested,
            this, &TaskManagerWindow::onUserHeaderContextMenu);

    ui.tree_processes->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.tree_services->setContextMenuPolicy(Qt::CustomContextMenu);
    ui.tree_users->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.tree_processes, &QTreeWidget::customContextMenuRequested,
            this, &TaskManagerWindow::onProcessContextMenu);
    connect(ui.tree_services, &QTreeWidget::customContextMenuRequested,
            this, &TaskManagerWindow::onServiceContextMenu);
    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &TaskManagerWindow::onUserContextMenu);

    connect(ui.tree_processes, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        ui.button_end_task->setEnabled(current != nullptr);
        ui.action_end_task->setEnabled(current != nullptr);
    });

    connect(ui.tree_users, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        ui.button_disconnect_user->setEnabled(current != nullptr);
        ui.action_disconnect_user->setEnabled(current != nullptr);
        ui.button_logoff->setEnabled(current != nullptr);
        ui.action_logoff_user->setEnabled(current != nullptr);
    });

    label_process_ = new QLabel(this);
    label_cpu_ = new QLabel(this);
    label_memory_ = new QLabel(this);

    QString labels_style = QStringLiteral("padding:3px;");
    label_process_->setStyleSheet(labels_style);
    label_cpu_->setStyleSheet(labels_style);
    label_memory_->setStyleSheet(labels_style);

    ui.statusbar->addWidget(label_process_);
    ui.statusbar->addWidget(label_cpu_);
    ui.statusbar->addWidget(label_memory_);

    TaskManagerSettings settings;
    restoreGeometry(settings.windowState());

    QByteArray process_state = settings.processColumnState();
    if (process_state.isEmpty())
    {
        ui.tree_processes->setColumnHidden(PROC_COL_SESSION_ID, true);
        ui.tree_processes->setColumnHidden(PROC_COL_MEM_WORKING_SET, true);
        ui.tree_processes->setColumnHidden(PROC_COL_MEM_PEAK_WORKING_SET, true);
        ui.tree_processes->setColumnHidden(PROC_COL_MEM_WORKING_SET_DELTA, true);
        ui.tree_processes->setColumnHidden(PROC_COL_THREAD_COUNT, true);
        ui.tree_processes->setColumnHidden(PROC_COL_IMAGE_PATH, true);

        ui.tree_processes->setColumnWidth(PROC_COL_NAME, 160);
        ui.tree_processes->setColumnWidth(PROC_COL_PROCESS_ID, 80);
        ui.tree_processes->setColumnWidth(PROC_COL_USER_NAME, 120);
        ui.tree_processes->setColumnWidth(PROC_COL_CPU_USAGE, 30);
        ui.tree_processes->setColumnWidth(PROC_COL_MEM_PRIVATE_WORKING_SET, 110);
    }
    else
    {
        ui.tree_processes->header()->restoreState(process_state);
    }

    QByteArray service_state = settings.serviceColumnState();
    if (service_state.isEmpty())
    {
        ui.tree_services->setColumnHidden(SERV_COL_DESCRIPTION, true);
        ui.tree_services->setColumnWidth(SERV_COL_NAME, 270);
    }
    else
    {
        ui.tree_services->header()->restoreState(service_state);
    }

    ui.tree_users->header()->restoreState(settings.userColumnState());

    ui.tree_processes->setSortingEnabled(true);
    ui.tree_processes->header()->setSortIndicatorShown(true);

    ui.tree_services->setSortingEnabled(true);
    ui.tree_services->header()->setSortIndicatorShown(true);

    ui.tree_users->setSortingEnabled(true);
    ui.tree_users->header()->setSortIndicatorShown(true);

    connect(ui.button_end_task, &QPushButton::clicked, this, &TaskManagerWindow::onEndProcess);
    connect(ui.button_disconnect_user, &QPushButton::clicked, this, &TaskManagerWindow::onDisconnectUser);
    connect(ui.button_logoff, &QPushButton::clicked, this, &TaskManagerWindow::onLogoffUser);
    connect(ui.action_end_task, &QAction::triggered, this, &TaskManagerWindow::onEndProcess);
    connect(ui.action_update, &QAction::triggered, this, [this]()
    {
        LOG(LS_INFO) << "[ACTION] Update";
        sendProcessListRequest(proto::task_manager::ProcessListRequest::RESET_CACHE);
        sendServiceListRequest();
        sendUserListRequest();
    });
    connect(ui.action_start_service, &QAction::triggered, this, &TaskManagerWindow::onStartService);
    connect(ui.action_stop_service, &QAction::triggered, this, &TaskManagerWindow::onStopService);
    connect(ui.action_disconnect_user, &QAction::triggered, this, &TaskManagerWindow::onDisconnectUser);
    connect(ui.action_logoff_user, &QAction::triggered, this, &TaskManagerWindow::onLogoffUser);

    update_timer_ = new QTimer(this);

    connect(update_timer_, &QTimer::timeout, this, [this]()
    {
        sendProcessListRequest(proto::task_manager::ProcessListRequest::NONE);
        sendUserListRequest();
    });

    std::chrono::milliseconds update_speed = settings.updateSpeed();
    if (update_speed == std::chrono::milliseconds(500))
        ui.action_high_speed->setChecked(true);
    else if (update_speed == std::chrono::milliseconds(1000))
        ui.action_medium_speed->setChecked(true);
    else if (update_speed == std::chrono::milliseconds(3000))
        ui.action_low_speed->setChecked(true);
    else if (update_speed == std::chrono::milliseconds(0))
        ui.action_disable_update->setChecked(true);

    update_timer_->start(update_speed);

    QTimer::singleShot(0, this, [this]()
    {
        sendProcessListRequest(proto::task_manager::ProcessListRequest::RESET_CACHE);
        sendServiceListRequest();
        sendUserListRequest();
    });
}

//--------------------------------------------------------------------------------------------------
TaskManagerWindow::~TaskManagerWindow()
{
    LOG(LS_INFO) << "Dtor";

    TaskManagerSettings settings;
    settings.setWindowState(saveGeometry());
    settings.setProcessColumnState(ui.tree_processes->header()->saveState());
    settings.setServiceColumnState(ui.tree_services->header()->saveState());
    settings.setUserColumnState(ui.tree_users->header()->saveState());

    if (!update_timer_->isActive())
        settings.setUpdateSpeed(std::chrono::milliseconds(0));
    else
        settings.setUpdateSpeed(std::chrono::milliseconds(update_timer_->interval()));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::readMessage(const proto::task_manager::HostToClient& message)
{
    if (message.has_process_list())
    {
        readProcessList(message.process_list());
    }
    else if (message.has_service_list())
    {
        readServiceList(message.service_list());
    }
    else if (message.has_user_list())
    {
        readUserList(message.user_list());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled task manager message";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::closeEvent(QCloseEvent* event)
{
    QMainWindow::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onProcessHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui.tree_processes->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui.tree_processes->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(
        menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onServiceHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui.tree_services->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui.tree_services->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(
        menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onUserHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui.tree_users->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui.tree_users->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(
        menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onProcessContextMenu(const QPoint& pos)
{
    ProcessItem* current_item = static_cast<ProcessItem*>(ui.tree_processes->itemAt(pos));
    if (current_item)
        ui.tree_processes->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_end_task);
    menu.addSeparator();
    addUpdateItems(&menu);
    menu.exec(ui.tree_processes->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onServiceContextMenu(const QPoint& pos)
{
    ServiceItem* current_item = static_cast<ServiceItem*>(ui.tree_services->itemAt(pos));
    if (current_item)
        ui.tree_services->setCurrentItem(current_item);

    QMenu menu;

    bool enable_start = false;
    bool enable_stop = false;

    if (current_item && current_item->startupType() != proto::task_manager::Service::STARTUP_TYPE_DISABLED)
    {
        proto::task_manager::Service::Status status = current_item->status();

        if (status == proto::task_manager::Service::STATUS_STOPPED)
            enable_start = true;
        else if (status == proto::task_manager::Service::STATUS_RUNNING)
            enable_stop = true;
    }

    ui.action_start_service->setEnabled(enable_start);
    ui.action_stop_service->setEnabled(enable_stop);

    menu.addAction(ui.action_start_service);
    menu.addAction(ui.action_stop_service);
    menu.addSeparator();
    addUpdateItems(&menu);
    menu.exec(ui.tree_services->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onUserContextMenu(const QPoint& pos)
{
    UserItem* current_item = static_cast<UserItem*>(ui.tree_users->itemAt(pos));
    if (current_item)
        ui.tree_users->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_disconnect_user);
    menu.addAction(ui.action_logoff_user);
    menu.addSeparator();
    addUpdateItems(&menu);
    menu.exec(ui.tree_users->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onEndProcess()
{
    LOG(LS_INFO) << "[ACTION] End process";

    ProcessItem* current_item = static_cast<ProcessItem*>(ui.tree_processes->currentItem());
    if (current_item)
    {
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you really want to end \"%1\" process?")
                                    .arg(current_item->text(PROC_COL_NAME)),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Accepted by user";
            sendEndProcessRequest(current_item->processId());
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Rejected by user";
        }
    }
    else
    {
        LOG(LS_INFO) << "No selected item";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onStartService()
{
    LOG(LS_INFO) << "[ACTION] Start service";

    ServiceItem* current_item = static_cast<ServiceItem*>(ui.tree_services->currentItem());
    if (current_item)
    {
        sendServiceRequest(current_item->name(), proto::task_manager::ServiceRequest::COMMAND_START);
    }
    else
    {
        LOG(LS_INFO) << "No selected item";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onStopService()
{
    LOG(LS_INFO) << "[ACTION] Stop service";

    ServiceItem* current_item = static_cast<ServiceItem*>(ui.tree_services->currentItem());
    if (current_item)
    {
        sendServiceRequest(current_item->name(), proto::task_manager::ServiceRequest::COMMAND_STOP);
    }
    else
    {
        LOG(LS_INFO) << "No selected item";
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onDisconnectUser()
{
    LOG(LS_INFO) << "[ACTION] Disconnect user";

    UserItem* current_item = static_cast<UserItem*>(ui.tree_users->currentItem());
    if (current_item)
    {
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you really want to disconnect user \"%1\" session?")
                                    .arg(current_item->text(USER_COL_NAME)),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Accepted by user";
            sendUserRequest(
                current_item->sessionId(), proto::task_manager::UserRequest::COMMAND_DISCONNECT);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Rejected by user";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::onLogoffUser()
{
    LOG(LS_INFO) << "[ACTION] Logoff user";

    UserItem* current_item = static_cast<UserItem*>(ui.tree_users->currentItem());
    if (current_item)
    {
        QMessageBox message_box(QMessageBox::Question,
                                tr("Confirmation"),
                                tr("Are you really want to end user \"%1\" session?")
                                    .arg(current_item->text(USER_COL_NAME)),
                                QMessageBox::Yes | QMessageBox::No,
                                this);
        message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
        message_box.button(QMessageBox::No)->setText(tr("No"));

        if (message_box.exec() == QMessageBox::Yes)
        {
            LOG(LS_INFO) << "[ACTION] Accepted by user";
            sendUserRequest(
                current_item->sessionId(), proto::task_manager::UserRequest::COMMAND_LOGOFF);
        }
        else
        {
            LOG(LS_INFO) << "[ACTION] Rejected by user";
        }
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendProcessListRequest(uint32_t flags)
{
    proto::task_manager::ClientToHost message;
    message.mutable_process_list_request()->set_flags(flags);
    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendEndProcessRequest(quint64 process_id)
{
    proto::task_manager::ClientToHost message;
    message.mutable_end_process_request()->set_pid(process_id);
    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendServiceListRequest()
{
    proto::task_manager::ClientToHost message;
    message.mutable_service_list_request()->set_dummy(1);
    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendServiceRequest(
    const std::string& name, proto::task_manager::ServiceRequest::Command command)
{
    proto::task_manager::ClientToHost message;

    proto::task_manager::ServiceRequest* service_request = message.mutable_service_request();
    service_request->set_name(name);
    service_request->set_command(command);

    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendUserListRequest()
{
    proto::task_manager::ClientToHost message;
    message.mutable_user_list_request()->set_dummy(1);
    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::sendUserRequest(
    uint32_t session_id, proto::task_manager::UserRequest::Command command)
{
    proto::task_manager::ClientToHost message;

    proto::task_manager::UserRequest* user_request = message.mutable_user_request();
    user_request->set_session_id(session_id);
    user_request->set_command(command);

    emit sig_sendMessage(message);
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::readProcessList(const proto::task_manager::ProcessList& process_list)
{
    // Remove dead processes from the list.
    for (int i = ui.tree_processes->topLevelItemCount() - 1; i >= 0; --i)
    {
        ProcessItem* temp = static_cast<ProcessItem*>(ui.tree_processes->topLevelItem(i));

        bool found = false;
        for (int j = 0; j < process_list.process_size(); ++j)
        {
            if (temp->processId() == process_list.process(j).process_id())
            {
                found = true;
                break;
            }
        }

        if (!found)
            delete temp;
    }

    // Adding or updating processes.
    for (int i = 0; i < process_list.process_size(); ++i)
    {
        const proto::task_manager::Process& process = process_list.process(i);
        ProcessItem* process_item = nullptr;

        for (int j = 0; j < ui.tree_processes->topLevelItemCount(); ++j)
        {
            ProcessItem* temp = static_cast<ProcessItem*>(ui.tree_processes->topLevelItem(j));
            if (temp->processId() == process.process_id())
            {
                process_item = temp;
                break;
            }
        }

        if (!process_item)
        {
            ui.tree_processes->addTopLevelItem(new ProcessItem(process));
        }
        else
        {
            process_item->updateItem(process);
        }
    }

    if (process_list.process_size() != ui.tree_processes->topLevelItemCount())
    {
        LOG(LS_ERROR) << "Number of processes mismatch (expected: "
                      << process_list.process_size() << " actual: "
                      << ui.tree_processes->topLevelItemCount() << ")";
    }

    setProcessCount(process_list.process_size());
    setCpuUsage(process_list.cpu_usage());
    setMemoryUsage(process_list.memory_usage());
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::readServiceList(const proto::task_manager::ServiceList& service_list)
{
    ui.tree_services->clear();

    QIcon item_icon(QStringLiteral(":/img/gear.png"));

    for (int i = 0; i < service_list.service_size(); ++i)
    {
        ServiceItem* item = new ServiceItem(service_list.service(i));
        item->setIcon(SERV_COL_NAME, item_icon);

        ui.tree_services->addTopLevelItem(item);
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::readUserList(const proto::task_manager::UserList& user_list)
{
    // Remove dead sessions from the list.
    for (int i = ui.tree_users->topLevelItemCount() - 1; i >= 0; --i)
    {
        UserItem* temp = static_cast<UserItem*>(ui.tree_users->topLevelItem(i));

        bool found = false;
        for (int j = 0; j < user_list.user_size(); ++j)
        {
            if (temp->sessionId() == user_list.user(j).session_id())
            {
                found = true;
                break;
            }
        }

        if (!found)
            delete temp;
    }

    // Update sessions list.
    for (int i = 0; i < user_list.user_size(); ++i)
    {
        const proto::task_manager::User& user = user_list.user(i);
        UserItem* user_item = nullptr;

        for (int j = 0; j < ui.tree_users->topLevelItemCount(); ++j)
        {
            UserItem* temp = static_cast<UserItem*>(ui.tree_users->topLevelItem(j));
            if (temp->sessionId() == user.session_id())
            {
                user_item = temp;
                break;
            }
        }

        if (user_item)
        {
            user_item->updateItem(user);
        }
        else
        {
            ui.tree_users->addTopLevelItem(new UserItem(user));
        }
    }
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::setProcessCount(int count)
{
    label_process_->setText(tr("Processes: %1").arg(count));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::setCpuUsage(int usage)
{
    label_cpu_->setText(tr("CPU loading: %1%").arg(usage));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::setMemoryUsage(int usage)
{
    label_memory_->setText(tr("Physical memory: %1%").arg(usage));
}

//--------------------------------------------------------------------------------------------------
void TaskManagerWindow::addUpdateItems(QMenu* parent_menu)
{
    QMenu* update_menu = parent_menu->addMenu(tr("Update Speed"));

    QActionGroup* update_group = new QActionGroup(update_menu);
    update_group->addAction(ui.action_high_speed);
    update_group->addAction(ui.action_medium_speed);
    update_group->addAction(ui.action_low_speed);
    update_group->addAction(ui.action_disable_update);

    connect(update_group, &QActionGroup::triggered, this, [this](QAction* action)
    {
        if (action == ui.action_high_speed)
            update_timer_->start(std::chrono::milliseconds(500));
        else if (action == ui.action_medium_speed)
            update_timer_->start(std::chrono::milliseconds(1000));
        else if (action == ui.action_low_speed)
            update_timer_->start(std::chrono::milliseconds(3000));
        else if (action == ui.action_disable_update)
            update_timer_->stop();
    });

    update_menu->addActions(update_group->actions());
    parent_menu->addAction(ui.action_update);
}

} // namespace client
