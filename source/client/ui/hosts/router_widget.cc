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

#include "client/ui/hosts/router_widget.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/router_user.h"
#include "client/ui/hosts/router_user_dialog.h"
#include "client/ui/hosts/router_workspace_dialog.h"
#include "common/ui/formatter.h"
#include "common/ui/msg_box.h"
#include "common/ui/status_dialog.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "ui_router_widget.h"

namespace {

class RelayTreeItem final : public QTreeWidgetItem
{
public:
    explicit RelayTreeItem(const proto::router::RelayInfo& info)
    {
        updateItem(info);

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setIcon(0, QIcon(":/img/stack.svg"));
        setText(0, QString::fromStdString(info.ip_address()));
        setText(1, time);

        const proto::peer::Version& version = info.version();

        setText(3, QString("%1.%2.%3").arg(version.major()).arg(version.minor()).arg(version.patch()));
        setText(4, QString::fromStdString(info.computer_name()));
        setText(5, QString::fromStdString(info.architecture()));
        setText(6, QString::fromStdString(info.os_name()));
    }

    void updateItem(const proto::router::RelayInfo& updated_info)
    {
        info = updated_info;
        setText(2, QString::number(info.pool_size()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        if (treeWidget()->sortColumn() == 1)
        {
            const RelayTreeItem* other_item = static_cast<const RelayTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::RelayInfo info;

private:
    Q_DISABLE_COPY_MOVE(RelayTreeItem)
};

class PeerTreeItem final : public QTreeWidgetItem
{
public:
    explicit PeerTreeItem(const proto::router::Peer& connection)
    {
        updateItem(connection);

        setIcon(0, QIcon(":/img/user.svg"));
        setText(0, QString::fromStdString(conn.client_user_name()));
        setText(1, QString::number(conn.host_id()));
        setText(2, QString::fromStdString(conn.host_address()));
        setText(3, QString::fromStdString(conn.client_address()));
    }

    void updateItem(const proto::router::Peer& connection)
    {
        conn = connection;
        setText(4, Formatter::sizeToString(conn.bytes_transferred()));
        setText(5, Formatter::delayToString(conn.duration()));
        setText(6, Formatter::delayToString(conn.idle_time()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 1)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.host_id() < other_item->conn.host_id();
        }
        else if (column == 4)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.bytes_transferred() < other_item->conn.bytes_transferred();
        }
        else if (column == 5)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.duration() < other_item->conn.duration();
        }
        else if (column == 6)
        {
            const PeerTreeItem* other_item = static_cast<const PeerTreeItem*>(&other);
            return conn.idle_time() < other_item->conn.idle_time();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::Peer conn;

private:
    Q_DISABLE_COPY_MOVE(PeerTreeItem)
};

class HostTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(HostTreeItem)

public:
    explicit HostTreeItem(const proto::router::Host& info)
    {
        updateItem(info);
    }

    void updateItem(const proto::router::Host& updated_info)
    {
        info = updated_info;

        const QString last_connect = info.last_connect() > 0
            ? QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(info.last_connect()), QLocale::ShortFormat)
            : QString();

        setIcon(0, QIcon(info.online() ? ":/img/computer-online.svg"
                                       : ":/img/computer-offline.svg"));
        setText(0, QString::number(info.host_id()));
        setText(1, QString::fromStdString(info.computer_name()));
        setText(2, QString::fromStdString(info.address()));
        setText(3, last_connect);
        setText(4, QString::fromStdString(info.version()));
        setText(5, QString::fromStdString(info.cpu_arch()));
        setText(6, QString::fromStdString(info.os_name()));
        setText(8, info.online() ? tr("Online") : tr("Offline"));
    }

    void setWorkspaceName(const QString& name)
    {
        setText(7, name);
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 0)
        {
            const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);
            return info.host_id() < other_item->info.host_id();
        }
        else if (column == 1)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(1), other.text(1)) <= 0;
        }
        else if (column == 3)
        {
            const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);
            return info.last_connect() < other_item->info.last_connect();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::Host info;

private:
    Q_DISABLE_COPY_MOVE(HostTreeItem)
};

class ClientTreeItem final : public QTreeWidgetItem
{
public:
    explicit ClientTreeItem(const proto::router::ClientInfo& info)
        : info(info)
    {
        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setIcon(0, QIcon(":/img/computer.svg"));
        setText(0, QString::fromStdString(info.computer_name()));
        setText(1, QString::fromStdString(info.ip_address()));
        setText(2, time);

        const proto::peer::Version& version = info.version();

        setText(3, QString("%1.%2.%3").arg(version.major()).arg(version.minor()).arg(version.patch()));
        setText(4, QString::fromStdString(info.architecture()));
        setText(5, QString::fromStdString(info.os_name()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 0)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(0), other.text(0)) <= 0;
        }
        else if (column == 2)
        {
            const ClientTreeItem* other_item = static_cast<const ClientTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::ClientInfo info;

private:
    Q_DISABLE_COPY_MOVE(ClientTreeItem)
};

class WorkspaceTreeItem final : public QTreeWidgetItem
{
public:
    explicit WorkspaceTreeItem(const proto::router::Workspace& workspace)
        : workspace(workspace)
    {
        setIcon(0, QIcon(":/img/workspace.svg"));
        setText(0, QString::fromStdString(workspace.name()));
    }

    void updateItem(const proto::router::Workspace& updated)
    {
        workspace = updated;
        setText(0, QString::fromStdString(workspace.name()));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        if (treeWidget()->sortColumn() == 0)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(0), other.text(0)) <= 0;
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::Workspace workspace;

private:
    Q_DISABLE_COPY_MOVE(WorkspaceTreeItem)
};

class UserTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(UserTreeItem)

public:
    explicit UserTreeItem(const proto::router::User& user)
        : user(RouterUser::parseFrom(user))
    {
        setText(0, QString::fromStdString(user.name()));
        setText(1, user.flags() & User::ENABLED ? tr("Yes") : tr("No"));
        setText(2, sessionsToString(user.sessions()));

        if (user.flags() & User::ENABLED)
            setIcon(0, QIcon(":/img/user.svg"));
        else
            setIcon(0, QIcon(":/img/locked-user.svg"));
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        int column = treeWidget()->sortColumn();
        if (column == 0)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(0), other.text(0)) <= 0;
        }
        else
        {
            return QTreeWidgetItem::operator<(other);
        }
    }

    QString sessionsToString(quint32 sessions)
    {
        QStringList list;

        if (sessions & proto::router::SESSION_TYPE_ADMIN)
            list.append(tr("Administrator"));
        if (sessions & proto::router::SESSION_TYPE_MANAGER)
            list.append(tr("Manager"));
        if (sessions & proto::router::SESSION_TYPE_CLIENT)
            list.append(tr("Client"));

        return list.join(", ");
    }

    RouterUser user;

private:
    Q_DISABLE_COPY_MOVE(UserTreeItem)
};

class ColumnAction : public QAction
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
    Q_DISABLE_COPY_MOVE(ColumnAction)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWidget::RouterWidget(const RouterConfig& config, QWidget* parent)
    : ContentWidget(Type::ROUTER, parent),
      ui(std::make_unique<Ui::RouterWidget>()),
      config_(config)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    status_dialog_ = new StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);
    status_dialog_->hide();

    router_ = new Router(config);
    router_->moveToThread(GuiApplication::ioThread());

    connect(router_, &Router::sig_statusChanged, this, &RouterWidget::onStatusChanged, Qt::QueuedConnection);
    connect(router_, &Router::sig_errorOccurred, this, &RouterWidget::onConnectionErrorOccurred, Qt::QueuedConnection);
    connect(router_, &Router::sig_relayListReceived, this, &RouterWidget::onRelayListReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_clientListReceived, this, &RouterWidget::onClientListReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_userListReceived, this, &RouterWidget::onUserListReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_userResultReceived, this, &RouterWidget::onUserResultReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_hostResultReceived, this, &RouterWidget::onHostResultReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_relayResultReceived, this, &RouterWidget::onRelayResultReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_clientResultReceived, this, &RouterWidget::onClientResultReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_workspaceListReceived, this, &RouterWidget::onWorkspaceListReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_workspaceResultReceived, this, &RouterWidget::onWorkspaceResultReceived, Qt::QueuedConnection);
    connect(router_, &Router::sig_hostListReceived, this, &RouterWidget::onHostListReceived, Qt::QueuedConnection);

    connect(this, &RouterWidget::sig_relayListRequest, router_, &Router::onRelayListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_clientListRequest, router_, &Router::onClientListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_userListRequest, router_, &Router::onUserListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_addUser, router_, &Router::onAddUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_modifyUser, router_, &Router::onModifyUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_deleteUser, router_, &Router::onDeleteUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectHost, router_, &Router::onDisconnectHost, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_removeHost, router_, &Router::onRemoveHost, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectRelay, router_, &Router::onDisconnectRelay, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectClient, router_, &Router::onDisconnectClient, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectPeer, router_, &Router::onDisconnectPeer, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_workspaceListRequest, router_, &Router::onWorkspaceListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_addWorkspace, router_, &Router::onAddWorkspace, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_modifyWorkspace, router_, &Router::onModifyWorkspace, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_deleteWorkspace, router_, &Router::onDeleteWorkspace, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_hostListRequest, router_, &Router::onHostListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_updateConfig, router_, &Router::onUpdateConfig, Qt::QueuedConnection);

    connect(ui->tab, &QTabWidget::currentChanged, this, &RouterWidget::onTabChanged);
    connect(ui->tree_users, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentUserChanged);
    connect(ui->tree_relays, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentRelayChanged);
    connect(ui->tree_hosts, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentHostChanged);
    connect(ui->tree_clients, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentClientChanged);
    connect(ui->tree_workspaces, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentWorkspaceChanged);
    connect(ui->tree_workspaces, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) { onModifyWorkspace(); });

    ui->tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onUserContextMenuRequested);

    ui->tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onHostContextMenuRequested);

    ui->tree_hosts->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterWidget::onHostsHeaderContextMenu);

    ui->tree_clients->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onClientContextMenuRequested);

    ui->tree_relays->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onRelayContextMenuRequested);

    ui->tree_peers->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_peers, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onPeerContextMenuRequested);

    ui->tree_workspaces->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_workspaces, &QTreeWidget::customContextMenuRequested, this, &RouterWidget::onWorkspaceContextMenuRequested);
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget()
{
    LOG(INFO) << "Dtor";

    if (router_)
        router_->disconnect(this);
}

//--------------------------------------------------------------------------------------------------
qint64 RouterWidget::routerId() const
{
    return config_.routerId();
}

//--------------------------------------------------------------------------------------------------
Router::Status RouterWidget::status() const
{
    return status_;
}

//--------------------------------------------------------------------------------------------------
RouterWidget::TabType RouterWidget::currentTabType() const
{
    return static_cast<TabType>(ui->tab->currentIndex());
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedUser() const
{
    return ui->tree_users->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedHost() const
{
    return ui->tree_hosts->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::hostCount() const
{
    return ui->tree_hosts->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentHostRow()
{
    QTreeWidgetItem* item = ui->tree_hosts->currentItem();
    if (!item)
        return;

    QString result;
    const int column_count = item->columnCount();
    for (int i = 0; i < column_count; ++i)
    {
        const QString text = item->text(i);
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentHostColumn(int column)
{
    QTreeWidgetItem* item = ui->tree_hosts->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedClient() const
{
    return ui->tree_clients->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::clientCount() const
{
    return ui->tree_clients->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentClientRow()
{
    QTreeWidgetItem* item = ui->tree_clients->currentItem();
    if (!item)
        return;

    QString result;
    const int column_count = item->columnCount();
    for (int i = 0; i < column_count; ++i)
    {
        const QString text = item->text(i);
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentClientColumn(int column)
{
    QTreeWidgetItem* item = ui->tree_clients->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedRelay() const
{
    return ui->tree_relays->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::relayCount() const
{
    return ui->tree_relays->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedWorkspace() const
{
    return ui->tree_workspaces->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::workspaceCount() const
{
    return ui->tree_workspaces->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentRelayRow()
{
    QTreeWidgetItem* item = ui->tree_relays->currentItem();
    if (!item)
        return;

    QString result;
    const int column_count = item->columnCount();
    for (int i = 0; i < column_count; ++i)
    {
        const QString text = item->text(i);
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentRelayColumn(int column)
{
    QTreeWidgetItem* item = ui->tree_relays->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::connectToRouter()
{
    if (router_)
    {
        QMetaObject::invokeMethod(
            router_, &Router::onConnectToRouter, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::disconnectFromRouter()
{
    if (router_)
    {
        QMetaObject::invokeMethod(
            router_, &Router::onDisconnectFromRouter, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateConfig(const RouterConfig& config)
{
    config_ = config;
    emit sig_updateConfig(config);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::showStatusDialog()
{
    status_dialog_->show();
    status_dialog_->activateWindow();
    status_dialog_->raise();
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_relays->header()->saveState();
        stream << ui->splitter->saveState();
        stream << ui->tree_peers->header()->saveState();
        stream << ui->tree_users->header()->saveState();
        stream << ui->tree_hosts->header()->saveState();
        stream << ui->tree_clients->header()->saveState();
        stream << ui->tree_workspaces->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray relays_columns_state;
    QByteArray splitter_state;
    QByteArray peers_columns_state;
    QByteArray users_columns_state;
    QByteArray hosts_columns_state;
    QByteArray clients_columns_state;
    QByteArray workspaces_columns_state;

    stream >> relays_columns_state;
    stream >> splitter_state;
    stream >> peers_columns_state;
    stream >> users_columns_state;
    stream >> hosts_columns_state;
    stream >> clients_columns_state;
    stream >> workspaces_columns_state;

    if (!relays_columns_state.isEmpty())
    {
        ui->tree_relays->header()->restoreState(relays_columns_state);
        ui->tree_relays->header()->setSectionsClickable(true);
        ui->tree_relays->header()->setSortIndicatorShown(true);
    }

    if (!splitter_state.isEmpty())
    {
        ui->splitter->restoreState(splitter_state);
    }
    else
    {
        int side_size = height() / 2;

        QList<int> sizes;
        sizes.emplace_back(side_size);
        sizes.emplace_back(side_size);
        ui->splitter->setSizes(sizes);
    }

    if (!peers_columns_state.isEmpty())
    {
        ui->tree_peers->header()->restoreState(peers_columns_state);
        ui->tree_peers->header()->setSectionsClickable(true);
        ui->tree_peers->header()->setSortIndicatorShown(true);
    }

    if (!users_columns_state.isEmpty())
    {
        ui->tree_users->header()->restoreState(users_columns_state);
        ui->tree_users->header()->setSectionsClickable(true);
        ui->tree_users->header()->setSortIndicatorShown(true);
    }

    if (!hosts_columns_state.isEmpty())
    {
        ui->tree_hosts->header()->restoreState(hosts_columns_state);
        ui->tree_hosts->header()->setSectionsClickable(true);
        ui->tree_hosts->header()->setSortIndicatorShown(true);
    }

    if (!clients_columns_state.isEmpty())
    {
        ui->tree_clients->header()->restoreState(clients_columns_state);
        ui->tree_clients->header()->setSectionsClickable(true);
        ui->tree_clients->header()->setSortIndicatorShown(true);
    }

    if (!workspaces_columns_state.isEmpty())
    {
        ui->tree_workspaces->header()->restoreState(workspaces_columns_state);
        ui->tree_workspaces->header()->setSectionsClickable(true);
        ui->tree_workspaces->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::reload()
{
    switch (currentTabType())
    {
        case TabType::WORKSPACES:
            onUpdateWorkspaceList();
            break;
        case TabType::HOSTS:
            onUpdateHostList();
            break;
        case TabType::CLIENTS:
            onUpdateClientList();
            break;
        case TabType::RELAYS:
            onUpdateRelayList();
            break;
        case TabType::USERS:
            onUpdateUserList();
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::canSave() const
{
    TabType tab = currentTabType();
    return tab == TabType::HOSTS || tab == TabType::CLIENTS || tab == TabType::RELAYS;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::save()
{
    switch (currentTabType())
    {
        case TabType::HOSTS:
            saveHostsToFile();
            break;
        case TabType::CLIENTS:
            saveClientsToFile();
            break;
        case TabType::RELAYS:
            saveRelaysToFile();
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    if (!status_label_)
        status_label_ = new QLabel(this);

    updateStatusLabel();

    statusbar->addWidget(status_label_);
    status_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar || !status_label_)
        return;

    statusbar->removeWidget(status_label_);
    status_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateRelayList()
{
    emit sig_relayListRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateHostList()
{
    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_ALL);
    emit sig_hostListRequest(request);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateClientList()
{
    emit sig_clientListRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateUserList()
{
    emit sig_userListRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onAddUser()
{
    QStringList names;
    for (int i = 0; i < ui->tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(i));
        names.append(item->user.name);
    }

    RouterUserDialog dialog(RouterUser(), names, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Add user rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Add user accepted by user";
    emit sig_addUser(dialog.user().serialize());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onModifyUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui->tree_users->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    QStringList names;
    for (int i = 0; i < ui->tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(i));
        if (item->text(0).compare(tree_item->text(0), Qt::CaseInsensitive) != 0)
            names.append(item->user.name);
    }

    RouterUserDialog dialog(tree_item->user, names, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(INFO) << "[ACTION] Modify user rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Modify user accepted by user";
    emit sig_modifyUser(dialog.user().serialize());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDeleteUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui->tree_users->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    qint64 entry_id = tree_item->user.entry_id;
    if (entry_id == 1)
    {
        LOG(INFO) << "Unable to delete built-in user";
        MsgBox::warning(this, tr("You cannot delete a built-in user."));
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to delete user \"%1\"?").arg(tree_item->text(0)))
        != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete user rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Delete user accepted by user";
    emit sig_deleteUser(entry_id);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectHost()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect host \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.computer_name()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect host accepted by user";
    emit sig_disconnectHost(tree_item->info.host_id());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectAllHosts()
{
    if (ui->tree_hosts->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Host list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all hosts?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all hosts rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect all hosts accepted by user";
    emit sig_disconnectHost(-1);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRemoveHost()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    MsgBox message_box(this);
    message_box.setWindowTitle(tr("Confirmation"));
    message_box.setText(tr("Deleting a host will result in all its configuration for connecting "
                           "to the router being deleted. This operation is irreversible. After "
                           "deleting, the host will no longer connect to the router. Are you sure "
                           "you want to do this?"));
    message_box.setIcon(MsgBox::Question);
    message_box.setStandardButtons(MsgBox::Yes | MsgBox::No);

    QCheckBox* check_box = new QCheckBox(&message_box);
    check_box->setText(tr("Try to uninstall the application (result is not guaranteed)"));
    message_box.setCheckBox(check_box);

    if (message_box.exec() == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Remove host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Remove host accepted by user";
    emit sig_removeHost(tree_item->info.host_id(), check_box->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectRelay()
{
    RelayTreeItem* tree_item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected relay";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect relay \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.ip_address()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect relay rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect relay accepted by user";
    emit sig_disconnectRelay(tree_item->info.entry_id());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectAllRelays()
{
    if (ui->tree_relays->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Relay list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all relays?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all relays rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect all relays accepted by user";
    emit sig_disconnectRelay(-1);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectClient()
{
    ClientTreeItem* tree_item = static_cast<ClientTreeItem*>(ui->tree_clients->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected client";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect client \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.computer_name()))) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect client rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect client accepted by user";
    emit sig_disconnectClient(tree_item->info.entry_id());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectAllClients()
{
    if (ui->tree_clients->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Client list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all clients?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all clients rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect all clients accepted by user";
    emit sig_disconnectClient(-1);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateWorkspaceList()
{
    emit sig_workspaceListRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onAddWorkspace()
{
    QStringList names;
    for (int i = 0; i < ui->tree_workspaces->topLevelItemCount(); ++i)
    {
        WorkspaceTreeItem* item = static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->topLevelItem(i));
        names.append(QString::fromStdString(item->workspace.name()));
    }

    QVector<RouterWorkspaceDialog::UserEntry> users;
    RouterWorkspaceDialog::SelfCredentials self;
    const QString self_user_name = config_.username();

    for (int i = 0; i < ui->tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(i));

        RouterWorkspaceDialog::UserEntry entry;
        entry.entry_id   = item->user.entry_id;
        entry.name       = item->user.name;
        entry.public_key = item->user.public_key;
        users.append(entry);

        if (item->user.name.compare(self_user_name, Qt::CaseInsensitive) == 0)
        {
            self.user_id          = item->user.entry_id;
            self.wrap_private_key = item->user.wrap_private_key;
            self.wrap_salt        = item->user.wrap_salt;
            self.password         = config_.password();
        }
    }

    RouterWorkspaceDialog dialog(proto::router::Workspace(), names, this);
    dialog.setUsers(users);
    dialog.setSelfCredentials(self);

    if (dialog.exec() != QDialog::Accepted)
        return;

    LOG(INFO) << "[ACTION] Add workspace accepted by user (access entries:"
              << dialog.workspace().access_size() << ")";
    emit sig_addWorkspace(dialog.workspace());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onModifyWorkspace()
{
    WorkspaceTreeItem* tree_item = static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected workspace";
        return;
    }

    const QString current_name = QString::fromStdString(tree_item->workspace.name());

    QStringList names;
    for (int i = 0; i < ui->tree_workspaces->topLevelItemCount(); ++i)
    {
        WorkspaceTreeItem* item = static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->topLevelItem(i));
        const QString item_name = QString::fromStdString(item->workspace.name());
        if (item_name.compare(current_name, Qt::CaseInsensitive) != 0)
            names.append(item_name);
    }

    QVector<RouterWorkspaceDialog::UserEntry> users;
    RouterWorkspaceDialog::SelfCredentials self;
    const QString self_user_name = config_.username();

    for (int i = 0; i < ui->tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(i));

        RouterWorkspaceDialog::UserEntry entry;
        entry.entry_id   = item->user.entry_id;
        entry.name       = item->user.name;
        entry.public_key = item->user.public_key;
        users.append(entry);

        if (item->user.name.compare(self_user_name, Qt::CaseInsensitive) == 0)
        {
            self.user_id          = item->user.entry_id;
            self.wrap_private_key = item->user.wrap_private_key;
            self.wrap_salt        = item->user.wrap_salt;
            self.password         = config_.password();
        }
    }

    RouterWorkspaceDialog dialog(tree_item->workspace, names, this);
    dialog.setUsers(users);
    dialog.setSelfCredentials(self);

    if (dialog.exec() != QDialog::Accepted)
        return;

    LOG(INFO) << "[ACTION] Modify workspace accepted by user (access entries:"
              << dialog.workspace().access_size() << ")";
    emit sig_modifyWorkspace(dialog.workspace());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDeleteWorkspace()
{
    WorkspaceTreeItem* tree_item = static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected workspace";
        return;
    }

    const QString name = QString::fromStdString(tree_item->workspace.name());
    if (MsgBox::question(this,
            tr("Are you sure you want to delete workspace \"%1\"?").arg(name)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete workspace rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Delete workspace accepted by user";
    emit sig_deleteWorkspace(tree_item->workspace.entry_id());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onStatusChanged(qint64 router_id, Router::Status status)
{
    status_ = status;

    switch (status)
    {
        case Router::Status::CONNECTING:
            status_dialog_->addMessage(tr("Connecting to router %1...").arg(config_.address()));
            break;
        case Router::Status::ONLINE:
            status_dialog_->addMessage(tr("Connection to router %1 established.").arg(config_.address()));
            break;
        case Router::Status::OFFLINE:
            status_dialog_->addMessage(tr("Disconnected from router %1.").arg(config_.address()));
            break;
    }

    if (status == Router::Status::ONLINE)
    {
        onUpdateRelayList();
        onUpdateHostList();
        onUpdateClientList();
        onUpdateUserList();
        onUpdateWorkspaceList();

        ui->tree_relays->setEnabled(true);
        ui->tree_peers->setEnabled(true);
        ui->tree_hosts->setEnabled(true);
        ui->tree_clients->setEnabled(true);
        ui->tree_users->setEnabled(true);
        ui->tree_workspaces->setEnabled(true);
    }
    else
    {
        ui->tree_relays->setEnabled(false);
        ui->tree_peers->setEnabled(false);
        ui->tree_hosts->setEnabled(false);
        ui->tree_clients->setEnabled(false);
        ui->tree_users->setEnabled(false);
        ui->tree_workspaces->setEnabled(false);

        ui->tree_relays->clear();
        ui->tree_peers->clear();
        ui->tree_hosts->clear();
        ui->tree_clients->clear();
        ui->tree_users->clear();
        ui->tree_workspaces->clear();

        updateStatusLabel();
    }

    emit sig_statusChanged(router_id, status);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onConnectionErrorOccurred(qint64 /* router_id */, TcpChannel::ErrorCode error_code)
{
    status_dialog_->addMessage(tr("Network error: %1.").arg(TcpChannel::errorToString(error_code)));
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onTabChanged(int index)
{
    updateStatusLabel();
    emit sig_currentTabTypeChanged(routerId(), static_cast<TabType>(index));
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentUserChanged()
{
    emit sig_currentUserChanged(routerId());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentRelayChanged()
{
    ui->tree_peers->clear();
    updateRelayStatistics();
    emit sig_currentRelayChanged(routerId());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentHostChanged()
{
    emit sig_currentHostChanged(routerId());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentClientChanged()
{
    emit sig_currentClientChanged(routerId());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentWorkspaceChanged()
{
    emit sig_currentWorkspaceChanged(routerId());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_users->itemAt(pos);
    if (item)
        ui->tree_users->setCurrentItem(item);

    User user;
    if (item)
        user = static_cast<UserTreeItem*>(item)->user;

    QPoint global_pos = ui->tree_users->viewport()->mapToGlobal(pos);
    emit sig_userContextMenu(routerId(), user, global_pos);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_hosts->itemAt(pos);
    if (item)
        ui->tree_hosts->setCurrentItem(item);

    const int column = ui->tree_hosts->indexAt(pos).column();
    const QPoint global_pos = ui->tree_hosts->viewport()->mapToGlobal(pos);
    emit sig_hostContextMenu(routerId(), global_pos, column);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostsHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_hosts->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_hosts->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onClientContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_clients->itemAt(pos);
    if (item)
        ui->tree_clients->setCurrentItem(item);

    const int column = ui->tree_clients->indexAt(pos).column();
    const QPoint global_pos = ui->tree_clients->viewport()->mapToGlobal(pos);
    emit sig_clientContextMenu(routerId(), global_pos, column);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_relays->itemAt(pos);
    if (item)
        ui->tree_relays->setCurrentItem(item);

    const int column = ui->tree_relays->indexAt(pos).column();
    const QPoint global_pos = ui->tree_relays->viewport()->mapToGlobal(pos);
    emit sig_relayContextMenu(routerId(), global_pos, column);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onPeerContextMenuRequested(const QPoint& pos)
{
    RelayTreeItem* relay_item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!relay_item)
        return;

    QTreeWidgetItem* item = ui->tree_peers->itemAt(pos);
    if (!item)
        return;

    ui->tree_peers->setCurrentItem(item);

    PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(item);
    const int column = ui->tree_peers->indexAt(pos).column();
    const QPoint global_pos = ui->tree_peers->viewport()->mapToGlobal(pos);

    QMenu menu;
    QAction* disconnect_action = menu.addAction(tr("Disconnect"));
    menu.addSeparator();
    QAction* copy_row_action = menu.addAction(tr("Copy Row"));
    QAction* copy_value_action = menu.addAction(tr("Copy Value"));

    QAction* selected = menu.exec(global_pos);
    if (!selected)
        return;

    if (selected == disconnect_action)
    {
        if (MsgBox::question(this,
                tr("Are you sure you want to disconnect peer \"%1\"?")
                    .arg(QString::fromStdString(peer_item->conn.client_user_name())))
            != MsgBox::Yes)
        {
            LOG(INFO) << "[ACTION] Disconnect peer rejected by user";
            return;
        }

        LOG(INFO) << "[ACTION] Disconnect peer accepted by user";
        emit sig_disconnectPeer(relay_item->info.entry_id(), peer_item->conn.session_id());
    }
    else if (selected == copy_row_action)
    {
        QString result;
        const int column_count = peer_item->columnCount();
        for (int i = 0; i < column_count; ++i)
        {
            const QString text = peer_item->text(i);
            if (!text.isEmpty())
                result += text + ' ';
        }
        result.chop(1);

        if (result.isEmpty())
            return;

        if (QClipboard* clipboard = QApplication::clipboard())
            clipboard->setText(result);
    }
    else if (selected == copy_value_action)
    {
        const QString text = peer_item->text(column);
        if (text.isEmpty())
            return;

        if (QClipboard* clipboard = QApplication::clipboard())
            clipboard->setText(text);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onWorkspaceContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_workspaces->itemAt(pos);
    if (item)
        ui->tree_workspaces->setCurrentItem(item);

    const QPoint global_pos = ui->tree_workspaces->viewport()->mapToGlobal(pos);
    emit sig_workspaceContextMenu(routerId(), global_pos);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayListReceived(const proto::router::RelayList& relays)
{
    auto has_with_id = [](const proto::router::RelayList& relays, qint64 entry_id)
    {
        for (int i = 0; i < relays.relay_size(); ++i)
        {
            if (relays.relay(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all relays that are not in the list.
    for (int i = ui->tree_relays->topLevelItemCount() - 1; i >= 0; --i)
    {
        RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(i));

        if (!has_with_id(relays, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < relays.relay_size(); ++i)
    {
        const proto::router::RelayInfo& info = relays.relay(i);
        bool found = false;

        for (int j = 0; j < ui->tree_relays->topLevelItemCount(); ++j)
        {
            RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_relays->addTopLevelItem(new RelayTreeItem(info));
    }

    updateRelayStatistics();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostListReceived(const proto::router::HostList& list)
{
    // The Hosts tab subscribes to the "any workspace / any group" response only; per-workspace
    // responses will be handled by future workspace widgets.
    if (list.workspace_id() != 0 || list.group_id() != 0)
        return;

    auto has_with_id = [](const proto::router::HostList& list, qint64 host_id)
    {
        for (int i = 0; i < list.host_size(); ++i)
        {
            if (list.host(i).host_id() == host_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui->tree_hosts->topLevelItemCount() - 1; i >= 0; --i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));

        if (!has_with_id(list, item->info.host_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < list.host_size(); ++i)
    {
        const proto::router::Host& info = list.host(i);
        HostTreeItem* target = nullptr;

        for (int j = 0; j < ui->tree_hosts->topLevelItemCount(); ++j)
        {
            HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(j));
            if (item->info.host_id() == info.host_id())
            {
                item->updateItem(info);
                target = item;
                break;
            }
        }

        if (!target)
        {
            target = new HostTreeItem(info);
            ui->tree_hosts->addTopLevelItem(target);
        }

        target->setWorkspaceName(workspaceNameById(info.workspace_id()));
    }

    emit sig_currentHostChanged(routerId());
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onClientListReceived(const proto::router::ClientList& clients)
{
    auto has_with_id = [](const proto::router::ClientList& clients, qint64 entry_id)
    {
        for (int i = 0; i < clients.client_size(); ++i)
        {
            if (clients.client(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all clients that are not in the list.
    for (int i = ui->tree_clients->topLevelItemCount() - 1; i >= 0; --i)
    {
        ClientTreeItem* item = static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(i));

        if (!has_with_id(clients, item->info.entry_id()))
            delete item;
    }

    // Adding new elements in the UI.
    for (int i = 0; i < clients.client_size(); ++i)
    {
        const proto::router::ClientInfo& info = clients.client(i);
        bool found = false;

        for (int j = 0; j < ui->tree_clients->topLevelItemCount(); ++j)
        {
            ClientTreeItem* item = static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_clients->addTopLevelItem(new ClientTreeItem(info));
    }

    emit sig_currentClientChanged(routerId());
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserListReceived(const proto::router::UserList& list)
{
    QTreeWidget* tree_users = ui->tree_users;

    qint64 selected_entry_id = 0;
    if (UserTreeItem* current = static_cast<UserTreeItem*>(tree_users->currentItem()))
        selected_entry_id = current->user.entry_id;

    tree_users->clear();

    UserTreeItem* to_select = nullptr;
    for (int i = 0; i < list.user_size(); ++i)
    {
        UserTreeItem* item = new UserTreeItem(list.user(i));
        tree_users->addTopLevelItem(item);

        if (item->user.entry_id == selected_entry_id)
            to_select = item;
    }

    if (to_select)
        tree_users->setCurrentItem(to_select);
    else
        emit sig_currentUserChanged(routerId());

    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserResultReceived(const proto::router::UserResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid user request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidData)
            message = QT_TR_NOOP("Invalid data was passed.");
        else if (error_code == proto::router::kErrorAlreadyExists)
            message = QT_TR_NOOP("A user with the specified name already exists.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    onUpdateUserList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostResultReceived(const proto::router::HostResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid host request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    onUpdateHostList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayResultReceived(const proto::router::RelayResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid relay request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    onUpdateRelayList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onClientResultReceived(const proto::router::ClientResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid client request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    onUpdateClientList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onWorkspaceListReceived(const proto::router::WorkspaceList& list)
{
    QTreeWidget* tree_workspaces = ui->tree_workspaces;

    qint64 selected_entry_id = 0;
    if (WorkspaceTreeItem* current = static_cast<WorkspaceTreeItem*>(tree_workspaces->currentItem()))
        selected_entry_id = current->workspace.entry_id();

    tree_workspaces->clear();

    WorkspaceTreeItem* to_select = nullptr;
    for (int i = 0; i < list.workspace_size(); ++i)
    {
        WorkspaceTreeItem* item = new WorkspaceTreeItem(list.workspace(i));
        tree_workspaces->addTopLevelItem(item);

        if (item->workspace.entry_id() == selected_entry_id)
            to_select = item;
    }

    if (to_select)
        tree_workspaces->setCurrentItem(to_select);
    else
        emit sig_currentWorkspaceChanged(routerId());

    refreshHostsWorkspaceColumn();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onWorkspaceResultReceived(const proto::router::WorkspaceResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid workspace request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidData)
            message = QT_TR_NOOP("Invalid data was passed.");
        else if (error_code == proto::router::kErrorAlreadyExists)
            message = QT_TR_NOOP("A workspace with the specified name already exists.");
        else if (error_code == proto::router::kErrorNotFound)
            message = QT_TR_NOOP("Workspace not found.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    onUpdateWorkspaceList();
}

//--------------------------------------------------------------------------------------------------
QString RouterWidget::workspaceNameById(qint64 workspace_id) const
{
    if (workspace_id <= 0)
        return QString();

    for (int i = 0; i < ui->tree_workspaces->topLevelItemCount(); ++i)
    {
        WorkspaceTreeItem* item =
            static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->topLevelItem(i));
        if (item->workspace.entry_id() == workspace_id)
            return QString::fromStdString(item->workspace.name());
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::refreshHostsWorkspaceColumn()
{
    for (int i = 0; i < ui->tree_hosts->topLevelItemCount(); ++i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));
        item->setWorkspaceName(workspaceNameById(item->info.workspace_id()));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateStatusLabel()
{
    if (!status_label_)
        return;

    switch (currentTabType())
    {
        case TabType::WORKSPACES:
            status_label_->setText(tr("%n workspace(s)", "", ui->tree_workspaces->topLevelItemCount()));
            break;
        case TabType::HOSTS:
            status_label_->setText(tr("%n host(s)", "", ui->tree_hosts->topLevelItemCount()));
            break;
        case TabType::CLIENTS:
            status_label_->setText(tr("%n client(s)", "", ui->tree_clients->topLevelItemCount()));
            break;
        case TabType::RELAYS:
            status_label_->setText(tr("%n relay(s)", "", ui->tree_relays->topLevelItemCount()));
            break;
        case TabType::USERS:
            status_label_->setText(tr("%n user(s)", "", ui->tree_users->topLevelItemCount()));
            break;
        default:
            status_label_->clear();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateRelayStatistics()
{
    RelayTreeItem* item = static_cast<RelayTreeItem*>(ui->tree_relays->currentItem());
    if (!item)
    {
        ui->tree_peers->setEnabled(false);
        return;
    }

    ui->tree_peers->setEnabled(true);

    if (!item->info.has_statistics())
        return;

    const proto::router::RelayInfo::Statistics& stats = item->info.statistics();

    auto has_with_id = [](const proto::router::RelayInfo::Statistics& stats, quint64 session_id)
    {
        for (int i = 0; i < stats.peer_size(); ++i)
        {
            if (stats.peer(i).session_id() == session_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all connections that are not in the list.
    for (int i = ui->tree_peers->topLevelItemCount() - 1; i >= 0; --i)
    {
        PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui->tree_peers->topLevelItem(i));
        if (!has_with_id(stats, peer_item->conn.session_id()))
            delete peer_item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < stats.peer_size(); ++i)
    {
        const proto::router::Peer& connection = stats.peer(i);
        bool found = false;

        for (int j = 0; j < ui->tree_peers->topLevelItemCount(); ++j)
        {
            PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui->tree_peers->topLevelItem(j));
            if (peer_item->conn.session_id() == connection.session_id())
            {
                peer_item->updateItem(connection);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_peers->addTopLevelItem(new PeerTreeItem(connection));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::saveHostsToFile()
{
    LOG(INFO) << "[ACTION] Save hosts to file";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save File"), QString(), tr("JSON files (*.json)"), &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected path";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui->tree_hosts->topLevelItemCount(); ++i)
    {
        const proto::router::Host& info =
            static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i))->info;

        QJsonObject host_object;

        host_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        host_object.insert("operating_system", QString::fromStdString(info.os_name()));
        host_object.insert("ip_address", QString::fromStdString(info.address()));
        host_object.insert("architecture", QString::fromStdString(info.cpu_arch()));
        host_object.insert("version", QString::fromStdString(info.version()));

        if (info.last_connect() > 0)
        {
            QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
                info.last_connect()), QLocale::ShortFormat);
            host_object.insert("connect_time", time);
        }

        host_object.insert("host_id", QString::number(info.host_id()));

        root_array.append(host_object);
    }

    QJsonObject root_object;
    root_object.insert("hosts", root_array);

    qint64 written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::saveClientsToFile()
{
    LOG(INFO) << "[ACTION] Save clients to file";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save File"), QString(), tr("JSON files (*.json)"), &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected path";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui->tree_clients->topLevelItemCount(); ++i)
    {
        const proto::router::ClientInfo& info =
            static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(i))->info;

        QJsonObject client_object;

        client_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        client_object.insert("operating_system", QString::fromStdString(info.os_name()));
        client_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        client_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3")
            .arg(info.version().major()).arg(info.version().minor()).arg(info.version().patch());
        client_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            info.timepoint()), QLocale::ShortFormat);
        client_object.insert("connect_time", time);

        root_array.append(client_object);
    }

    QJsonObject root_object;
    root_object.insert("clients", root_array);

    qint64 written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::saveRelaysToFile()
{
    LOG(INFO) << "[ACTION] Save relays to file";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save File"), QString(), tr("JSON files (*.json)"), &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected path";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui->tree_relays->topLevelItemCount(); ++i)
    {
        const proto::router::RelayInfo& info =
            static_cast<RelayTreeItem*>(ui->tree_relays->topLevelItem(i))->info;

        QJsonObject relay_object;

        relay_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        relay_object.insert("operating_system", QString::fromStdString(info.os_name()));
        relay_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        relay_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3")
            .arg(info.version().major()).arg(info.version().minor()).arg(info.version().patch());
        relay_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            info.timepoint()), QLocale::ShortFormat);
        relay_object.insert("connect_time", time);

        relay_object.insert("pool_size", QString::number(info.pool_size()));

        if (info.has_statistics())
        {
            const proto::router::RelayInfo::Statistics& stats = info.statistics();

            QJsonArray active_array;
            for (int j = 0; j < stats.peer_size(); ++j)
            {
                const proto::router::Peer& conn = stats.peer(j);

                QJsonObject conn_object;
                conn_object.insert("host_address", QString::fromStdString(conn.host_address()));
                conn_object.insert("host_id", QString::number(conn.host_id()));
                conn_object.insert("client_address", QString::fromStdString(conn.client_address()));
                conn_object.insert("user_name", QString::fromStdString(conn.client_user_name()));
                conn_object.insert("bytes_transferred", static_cast<long long>(conn.bytes_transferred()));
                conn_object.insert("duration", static_cast<long long>(conn.duration()));
                conn_object.insert("idle", static_cast<long long>(conn.idle_time()));

                active_array.append(conn_object);
            }

            relay_object.insert("connections", active_array);
            relay_object.insert("uptime", static_cast<long long>(stats.uptime()));
        }

        root_array.append(relay_object);
    }

    QJsonObject root_object;
    root_object.insert("relays", root_array);

    qint64 written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

