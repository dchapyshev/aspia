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
#include <QClipboard>
#include <QCollator>
#include <QComboBox>
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
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>

#include "base/crypto/secure_string.h"
#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "client/router.h"
#include "client/ui/hosts/router_user_dialog.h"
#include "client/ui/hosts/router_workspace_dialog.h"
#include "common/ui/credentials_dialog.h"
#include "common/ui/formatter.h"
#include "common/ui/icon_text_button.h"
#include "common/ui/msg_box.h"
#include "proto/router_constants.h"
#include "common/ui/status_dialog.h"
#include "common/ui/two_factor_code_dialog.h"
#include "common/ui/two_factor_enroll_dialog.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "ui_router_widget.h"

namespace {

enum HostColumn
{
    HOST_COLUMN_HOST_ID = 0,
    HOST_COLUMN_DISPLAY_NAME,
    HOST_COLUMN_COMPUTER_NAME,
    HOST_COLUMN_ADDRESS,
    HOST_COLUMN_USER_NAME,
    HOST_COLUMN_COMMENT,
    HOST_COLUMN_WORKSPACE,
    HOST_COLUMN_OS,
    HOST_COLUMN_VERSION,
    HOST_COLUMN_ARCH,
    HOST_COLUMN_LAST_CONNECT,
    HOST_COLUMN_LAST_MODIFY,
    HOST_COLUMN_STATUS
};

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
    explicit HostTreeItem(const Router::Host& host)
    {
        updateItem(host);
    }

    void updateItem(const Router::Host& updated_host)
    {
        info = updated_host;

        QString name = info.display_name;
        if (name.isEmpty())
            name = info.computer_name;

        const QString last_connect = info.last_connect > 0
            ? QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(info.last_connect), QLocale::ShortFormat)
            : QString();
        const QString last_modify = info.last_modify > 0
            ? QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(info.last_modify), QLocale::ShortFormat)
            : QString();

        setIcon(HOST_COLUMN_HOST_ID, QIcon(info.online ? ":/img/computer-online.svg"
                                                        : ":/img/computer-offline.svg"));
        setText(HOST_COLUMN_HOST_ID, QString::number(info.host_id));
        setText(HOST_COLUMN_DISPLAY_NAME, name);
        setText(HOST_COLUMN_COMPUTER_NAME, info.computer_name);
        setText(HOST_COLUMN_ADDRESS, info.address);
        setText(HOST_COLUMN_USER_NAME, info.user_name);
        setText(HOST_COLUMN_COMMENT, info.comment);
        setText(HOST_COLUMN_OS, info.os_name);
        setText(HOST_COLUMN_VERSION, info.version);
        setText(HOST_COLUMN_ARCH, info.cpu_arch);
        setText(HOST_COLUMN_LAST_CONNECT, last_connect);
        setText(HOST_COLUMN_LAST_MODIFY, last_modify);
        setText(HOST_COLUMN_STATUS, info.online ? tr("Online") : tr("Offline"));
    }

    void setWorkspaceName(const QString& name)
    {
        setText(HOST_COLUMN_WORKSPACE, name);
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        const int column = treeWidget()->sortColumn();
        const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);

        if (column == HOST_COLUMN_HOST_ID)
            return info.host_id < other_item->info.host_id;
        if (column == HOST_COLUMN_LAST_CONNECT)
            return info.last_connect < other_item->info.last_connect;
        if (column == HOST_COLUMN_LAST_MODIFY)
            return info.last_modify < other_item->info.last_modify;
        if (column == HOST_COLUMN_DISPLAY_NAME)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(HOST_COLUMN_DISPLAY_NAME),
                                    other.text(HOST_COLUMN_DISPLAY_NAME)) <= 0;
        }

        return QTreeWidgetItem::operator<(other);
    }

    Router::Host info;

private:
    Q_DISABLE_COPY_MOVE(HostTreeItem)
};

class ClientTreeItem final : public QTreeWidgetItem
{
public:
    explicit ClientTreeItem(const proto::router::ClientInfo& info)
    {
        setIcon(0, QIcon(":/img/computer.svg"));
        updateItem(info);
    }

    void updateItem(const proto::router::ClientInfo& updated_info)
    {
        info = updated_info;

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

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
    explicit WorkspaceTreeItem(const Router::Workspace& workspace)
    {
        setIcon(0, QIcon(":/img/workspace.svg"));
        updateItem(workspace);
    }

    void updateItem(const Router::Workspace& updated_workspace)
    {
        workspace = updated_workspace;

        QString single_line_comment = workspace.comment;
        single_line_comment.replace('\n', ' ').replace('\r', ' ');

        setText(0, workspace.name);
        setText(1, single_line_comment);
        setToolTip(1, workspace.comment);
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

    Router::Workspace workspace;

private:
    Q_DISABLE_COPY_MOVE(WorkspaceTreeItem)
};

class UserTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(UserTreeItem)

public:
    explicit UserTreeItem(const proto::router::User& user)
    {
        updateItem(user);
    }

    void updateItem(const proto::router::User& updated_user)
    {
        user = RouterUser::parseFrom(updated_user);

        setText(0, QString::fromStdString(updated_user.name()));
        setText(1, updated_user.flags() & User::ENABLED ? tr("Yes") : tr("No"));
        setText(2, sessionsToString(updated_user.sessions()));

        if (updated_user.flags() & User::ENABLED)
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
      router_(new Router(config, this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    status_dialog_ = new StatusDialog(this);
    status_dialog_->setWindowFlag(Qt::WindowStaysOnTopHint);
    status_dialog_->hide();

    connect(router_, &Router::sig_statusChanged, this, &RouterWidget::onStatusChanged);
    connect(router_, &Router::sig_errorOccurred, this, &RouterWidget::onConnectionErrorOccurred);
    connect(router_, &Router::sig_passwordChangeRequired, this, &RouterWidget::onPasswordChangeRequired);
    connect(router_, &Router::sig_twoFactorCodeRequired, this, &RouterWidget::onTwoFactorCodeRequired);
    connect(router_, &Router::sig_twoFactorEnrollment, this, &RouterWidget::onTwoFactorEnrollment);
    connect(router_, &Router::sig_hostsChanged, this, &RouterWidget::onUpdateHostList);
    connect(router_, &Router::sig_relaysChanged, this, &RouterWidget::onUpdateRelayList);
    connect(router_, &Router::sig_clientsChanged, this, &RouterWidget::onUpdateClientList);
    connect(router_, &Router::sig_usersChanged, this, &RouterWidget::onUpdateUserList);
    connect(router_, &Router::sig_workspacesChanged, this, &RouterWidget::onUpdateWorkspaceList);

    connect(ui->tab, &QTabWidget::currentChanged, this, &RouterWidget::onTabChanged);
    connect(ui->tree_users, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentUserChanged);
    connect(ui->tree_relays, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentRelayChanged);
    connect(ui->tree_hosts, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentHostChanged);
    connect(ui->tree_clients, &QTreeWidget::itemSelectionChanged, this, &RouterWidget::onCurrentClientChanged);

    connect(ui->tree_workspaces, &QTreeWidget::itemSelectionChanged,
            this, &RouterWidget::onCurrentWorkspaceChanged);
    connect(ui->tree_workspaces, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem*, int) { onModifyWorkspace(); });

    ui->tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onUserContextMenuRequested);

    ui->tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onHostContextMenuRequested);

    ui->tree_hosts->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tree_hosts->header()->setSectionHidden(HOST_COLUMN_USER_NAME, true);
    ui->tree_hosts->header()->setSectionHidden(HOST_COLUMN_COMMENT, true);
    connect(ui->tree_hosts->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterWidget::onHostsHeaderContextMenu);

    ui->tree_clients->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_clients, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onClientContextMenuRequested);

    ui->tree_relays->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_relays, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onRelayContextMenuRequested);

    ui->tree_peers->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_peers, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onPeerContextMenuRequested);

    ui->tree_workspaces->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_workspaces, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onWorkspaceContextMenuRequested);

    ui->combo_hosts_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_hosts_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_hosts_page_size->addItem("200", QVariant::fromValue<qint64>(200));
    ui->combo_hosts_page_size->setCurrentIndex(1);

    ui->button_hosts_next->setIconOnRight(true);

    connect(ui->combo_hosts_page_size, &QComboBox::currentIndexChanged,
            this, &RouterWidget::onHostsPageSizeChanged);
    connect(ui->combo_hosts_page, &QComboBox::currentIndexChanged,
            this, &RouterWidget::onHostsPageChanged);
    connect(ui->button_hosts_prev, &QToolButton::clicked, this, &RouterWidget::onHostsPrevClicked);
    connect(ui->button_hosts_next, &QToolButton::clicked, this, &RouterWidget::onHostsNextClicked);

    updateHostsPagination();
    syncAdminVisibility();
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::syncAdminVisibility()
{
    const bool is_admin = router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
    ui->tab->setVisible(is_admin);
    ui->label_non_admin_hint->setVisible(!is_admin);
}

//--------------------------------------------------------------------------------------------------
qint64 RouterWidget::routerId() const
{
    return router_->routerId();
}

//--------------------------------------------------------------------------------------------------
Router::Status RouterWidget::status() const
{
    return router_->status();
}

//--------------------------------------------------------------------------------------------------
RouterWidget::TabType RouterWidget::currentTabType() const
{
    if (router_->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return TabType::NONE;
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
bool RouterWidget::isSelectedHostOnline() const
{
    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    return item && item->info.online;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterWidget::selectedHostConfig() const
{
    HostConfig config;

    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!item || item->info.host_id == kInvalidHostId)
        return config;

    const Router::Host& host = item->info;

    QString name = host.display_name;
    if (name.isEmpty())
        name = host.computer_name;

    config.setRouterId(router_->routerId());
    config.setAddress(hostIdToString(host.host_id));
    config.setName(name);
    config.setUsername(host.user_name);
    config.setPassword(host.password);
    return config;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::hostCount() const
{
    return ui->tree_hosts->topLevelItemCount();
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
QByteArray RouterWidget::saveState()
{
    if (router_->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return QByteArray();

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
    if (router_->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

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
bool RouterWidget::canReload() const
{
    return router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
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

    // Non-admin sessions have no tab with row counts to report, so leave the status bar empty.
    if (router_->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
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
void RouterWidget::connectToRouter()
{
    router_->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::disconnectFromRouter()
{
    router_->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateConfig(const RouterConfig& config)
{
    router_->updateConfig(config);
    syncAdminVisibility();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::showStatusDialog()
{
    status_dialog_->show();
    status_dialog_->activateWindow();
    status_dialog_->raise();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateRelayList()
{
    if (router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
        router_->listRelays(this, &RouterWidget::onRelayListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateHostList()
{
    const qint64 start = (hosts_current_page_ - 1) * hosts_page_size_;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_ALL);
    request.set_start_item(start);
    request.set_end_item(start + hosts_page_size_ - 1);
    router_->listHosts(std::move(request), this, &RouterWidget::onHostListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateClientList()
{
    if (router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
        router_->listClients(this, &RouterWidget::onClientListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateUserList()
{
    if (router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
        router_->listUsers(this, &RouterWidget::onUserListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onAddUser()
{
    RouterUserDialog dialog(router_->routerId(), 0, this);
    if (dialog.exec() == QDialog::Accepted)
        onUpdateUserList();
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

    RouterUserDialog dialog(router_->routerId(), tree_item->user.entry_id, this);
    if (dialog.exec() == QDialog::Accepted)
        onUpdateUserList();
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
    router_->deleteUser(entry_id, this, &RouterWidget::onUserResultReceived);
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
        .arg(tree_item->info.computer_name)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect host accepted by user";
    router_->disconnectHost(tree_item->info.host_id, this, &RouterWidget::onHostResultReceived);
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
    router_->disconnectHost(kAllHostsId, this, &RouterWidget::onHostResultReceived);
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
                           "to the router being deleted, and the application will be uninstalled "
                           "on the host. This operation is irreversible. Are you sure you want to "
                           "do this?"));
    message_box.setIcon(MsgBox::Question);
    message_box.setStandardButtons(MsgBox::Yes | MsgBox::No);

    if (message_box.exec() == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Remove host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Remove host accepted by user";
    router_->removeHost(tree_item->info.host_id, this, &RouterWidget::onHostResultReceived);
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
    router_->disconnectRelay(tree_item->info.entry_id(), this, &RouterWidget::onRelayResultReceived);
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
    router_->disconnectRelay(-1, this, &RouterWidget::onRelayResultReceived);
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
    router_->disconnectClient(tree_item->info.entry_id(), this, &RouterWidget::onClientResultReceived);
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
    router_->disconnectClient(-1, this, &RouterWidget::onClientResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateWorkspaceList()
{
    router_->listWorkspaces(0, this, &RouterWidget::onWorkspaceListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onAddWorkspace()
{
    RouterWorkspaceDialog dialog(router_->routerId(), 0, this);
    if (dialog.exec() == QDialog::Accepted)
        onUpdateWorkspaceList();
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

    RouterWorkspaceDialog dialog(router_->routerId(), tree_item->workspace.entry_id, this);
    if (dialog.exec() == QDialog::Accepted)
        onUpdateWorkspaceList();
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

    const QString name = tree_item->workspace.name;
    if (MsgBox::question(this,
            tr("Are you sure you want to delete workspace \"%1\"?").arg(name)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete workspace rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Delete workspace accepted by user";
    router_->deleteWorkspace(tree_item->workspace.entry_id, this, &RouterWidget::onWorkspaceResultReceived);
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
    switch (status)
    {
        case Router::Status::CONNECTING:
            status_dialog_->addMessage(tr("Connecting to router %1...").arg(router_->config().address()));
            break;
        case Router::Status::ONLINE:
            status_dialog_->addMessage(tr("Connection to router %1 established.").arg(router_->config().address()));
            break;
        case Router::Status::OFFLINE:
            status_dialog_->addMessage(tr("Disconnected from router %1.").arg(router_->config().address()));
            break;
    }

    if (router_->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
    {
        if (status == Router::Status::ONLINE)
        {
            // Workspaces must be fetched first so the GK cache is populated by the time the
            // host list response arrives; otherwise the encrypted host fields stay empty.
            onUpdateWorkspaceList();
            onUpdateRelayList();
            onUpdateHostList();
            onUpdateClientList();
            onUpdateUserList();

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
    }

    emit sig_statusChanged(router_id, status);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onConnectionErrorOccurred(qint64 /* router_id */, TcpChannel::ErrorCode error_code)
{
    status_dialog_->addMessage(tr("Network error: %1.").arg(TcpChannel::errorToString(error_code)));
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onPasswordChangeRequired()
{
    MsgBox::information(this,
        tr("To complete the migration from a previous version, you need to change your password."));

    qint64 router_id = router_->routerId();
    CredentialsDialog dialog(CredentialsDialog::Type::SET_PASSWORD, this);
    dialog.setWindowTitle(tr("Change Password"));
    dialog.setValidator([router_id](CredentialsDialog* d) -> bool
    {
        SecureString password = d->password();

        if (!User::isValidPassword(password))
        {
            MsgBox::warning(d, tr("Password can not be empty and should not exceed %n characters.",
                "", User::kMaxPasswordLength));
            return false;
        }

        if (!User::isSafePassword(password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                "numbers and should not be shorter than %n characters.",
                "", User::kSafePasswordLength);
            QString question = tr("Do you want to enter a different password?");

            if (MsgBox::warning(d, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                return false;
        }

        Router* router = Router::instance(router_id);
        if (!router)
        {
            LOG(ERROR) << "Router instance is gone";
            return false;
        }

        d->setEnabled(false);
        router->changePassword(password, d, [d](const proto::router::ChangePasswordResult& result)
        {
            const std::string& error_code = result.error_code();
            if (error_code == proto::router::kErrorOk)
            {
                d->accept();
                return;
            }

            const char* message;
            if (error_code == proto::router::kErrorInvalidRequest)
                message = QT_TR_NOOP("Invalid password change request.");
            else if (error_code == proto::router::kErrorInternalError)
                message = QT_TR_NOOP("Unknown internal error.");
            else if (error_code == proto::router::kErrorInvalidData)
                message = QT_TR_NOOP("Invalid data was passed.");
            else
                message = QT_TR_NOOP("Unknown error type.");

            d->setEnabled(true);
            MsgBox::warning(d, RouterWidget::tr(message));
        });

        return false;
    });

    if (dialog.exec() == QDialog::Accepted)
        status_dialog_->addMessage(tr("Password updated. Waiting for new encryption keys..."));
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onTwoFactorCodeRequired()
{
    TwoFactorCodeDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        router_->submitTwoFactorCode(dialog.code());
    else
        router_->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onTwoFactorEnrollment(qint64 /* router_id */, const QString& otpauth_uri)
{
    TwoFactorEnrollDialog dialog(otpauth_uri, this);
    if (dialog.exec() == QDialog::Accepted)
        router_->submitTwoFactorCode(dialog.code());
    else
        router_->disconnectFromRouter();
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
        router_->disconnectPeer(relay_item->info.entry_id(), peer_item->conn.peer_id(),
            this, &RouterWidget::onPeerResultReceived);
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
void RouterWidget::onHostListReceived(const Router::HostList& list)
{
    // The Hosts tab subscribes to the "any workspace / any group" response only; per-workspace
    // responses are handled by RouterGroupWidget.
    if (list.workspace_id != 0 || list.group_id != 0)
        return;

    auto has_with_id = [](const Router::HostList& list, HostId host_id)
    {
        for (const Router::Host& host : std::as_const(list.hosts))
        {
            if (host.host_id == host_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui->tree_hosts->topLevelItemCount() - 1; i >= 0; --i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));

        if (!has_with_id(list, item->info.host_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (const Router::Host& host : std::as_const(list.hosts))
    {
        HostTreeItem* target = nullptr;

        for (int j = 0; j < ui->tree_hosts->topLevelItemCount(); ++j)
        {
            HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(j));
            if (item->info.host_id == host.host_id)
            {
                item->updateItem(host);
                target = item;
                break;
            }
        }

        if (!target)
        {
            target = new HostTreeItem(host);
            ui->tree_hosts->addTopLevelItem(target);
        }

        target->setWorkspaceName(workspaceNameById(host.workspace_id));
    }

    hosts_total_count_ = list.total_count;
    updateHostsPagination();

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

    // Adding and updating elements in the UI.
    for (int i = 0; i < clients.client_size(); ++i)
    {
        const proto::router::ClientInfo& info = clients.client(i);
        bool found = false;

        for (int j = 0; j < ui->tree_clients->topLevelItemCount(); ++j)
        {
            ClientTreeItem* item = static_cast<ClientTreeItem*>(ui->tree_clients->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
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
    auto has_with_id = [](const proto::router::UserList& list, qint64 entry_id)
    {
        for (int i = 0; i < list.user_size(); ++i)
        {
            if (list.user(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all users that are not in the list.
    for (int i = ui->tree_users->topLevelItemCount() - 1; i >= 0; --i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(i));

        if (!has_with_id(list, item->user.entry_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < list.user_size(); ++i)
    {
        const proto::router::User& info = list.user(i);
        bool found = false;

        for (int j = 0; j < ui->tree_users->topLevelItemCount(); ++j)
        {
            UserTreeItem* item = static_cast<UserTreeItem*>(ui->tree_users->topLevelItem(j));
            if (item->user.entry_id == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_users->addTopLevelItem(new UserTreeItem(info));
    }

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
void RouterWidget::onPeerResultReceived(const proto::router::PeerResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorNotFound)
            message = QT_TR_NOOP("Relay session not found.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onWorkspaceListReceived(const Router::WorkspaceList& list)
{
    auto has_with_id = [](const Router::WorkspaceList& list, qint64 entry_id)
    {
        for (const Router::Workspace& workspace : std::as_const(list.workspaces))
        {
            if (workspace.entry_id == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all workspaces that are not in the list.
    for (int i = ui->tree_workspaces->topLevelItemCount() - 1; i >= 0; --i)
    {
        WorkspaceTreeItem* item =
            static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->topLevelItem(i));

        if (!has_with_id(list, item->workspace.entry_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (const Router::Workspace& workspace : std::as_const(list.workspaces))
    {
        bool found = false;

        for (int j = 0; j < ui->tree_workspaces->topLevelItemCount(); ++j)
        {
            WorkspaceTreeItem* item =
                static_cast<WorkspaceTreeItem*>(ui->tree_workspaces->topLevelItem(j));
            if (item->workspace.entry_id == workspace.entry_id)
            {
                item->updateItem(workspace);
                found = true;
                break;
            }
        }

        if (!found)
            ui->tree_workspaces->addTopLevelItem(new WorkspaceTreeItem(workspace));
    }

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
void RouterWidget::onHostsPageSizeChanged(int /* index */)
{
    hosts_page_size_ = ui->combo_hosts_page_size->currentData().toLongLong();
    hosts_current_page_ = 1;
    onUpdateHostList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostsPageChanged(int index)
{
    if (index < 0)
        return;

    const qint64 page = index + 1;
    if (page == hosts_current_page_)
        return;

    hosts_current_page_ = page;
    onUpdateHostList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostsPrevClicked()
{
    if (hosts_current_page_ <= 1)
        return;

    --hosts_current_page_;
    onUpdateHostList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostsNextClicked()
{
    const qint64 total_pages =
        (hosts_total_count_ + hosts_page_size_ - 1) / hosts_page_size_;
    if (hosts_current_page_ >= total_pages)
        return;

    ++hosts_current_page_;
    onUpdateHostList();
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
        if (item->workspace.entry_id == workspace_id)
            return item->workspace.name;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::refreshHostsWorkspaceColumn()
{
    for (int i = 0; i < ui->tree_hosts->topLevelItemCount(); ++i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));
        item->setWorkspaceName(workspaceNameById(item->info.workspace_id));
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

    auto has_with_id = [](const proto::router::RelayInfo::Statistics& stats, qint64 peer_id)
    {
        for (int i = 0; i < stats.peer_size(); ++i)
        {
            if (stats.peer(i).peer_id() == peer_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all connections that are not in the list.
    for (int i = ui->tree_peers->topLevelItemCount() - 1; i >= 0; --i)
    {
        PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui->tree_peers->topLevelItem(i));
        if (!has_with_id(stats, peer_item->conn.peer_id()))
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
            if (peer_item->conn.peer_id() == connection.peer_id())
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
void RouterWidget::updateHostsPagination()
{
    qint64 total_pages = 1;
    if (hosts_total_count_ > 0)
        total_pages = (hosts_total_count_ + hosts_page_size_ - 1) / hosts_page_size_;

    if (hosts_current_page_ > total_pages)
        hosts_current_page_ = total_pages;
    if (hosts_current_page_ < 1)
        hosts_current_page_ = 1;

    QSignalBlocker blocker(ui->combo_hosts_page);
    ui->combo_hosts_page->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        ui->combo_hosts_page->addItem(QString::number(i));
    ui->combo_hosts_page->setCurrentIndex(static_cast<int>(hosts_current_page_ - 1));

    ui->combo_hosts_page->setEnabled(total_pages > 1);
    ui->button_hosts_prev->setEnabled(hosts_current_page_ > 1);
    ui->button_hosts_next->setEnabled(hosts_current_page_ < total_pages);
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
        const Router::Host& info =
            static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i))->info;

        QJsonObject host_object;

        host_object.insert("display_name", info.display_name);
        host_object.insert("computer_name", info.computer_name);
        host_object.insert("operating_system", info.os_name);
        host_object.insert("ip_address", info.address);
        host_object.insert("user_name", info.user_name);
        host_object.insert("comment", info.comment);
        host_object.insert("workspace", workspaceNameById(info.workspace_id));
        host_object.insert("architecture", info.cpu_arch);
        host_object.insert("version", info.version);

        if (info.last_connect > 0)
        {
            QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
                info.last_connect), QLocale::ShortFormat);
            host_object.insert("connect_time", time);
        }

        if (info.last_modify > 0)
        {
            QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
                info.last_modify), QLocale::ShortFormat);
            host_object.insert("modify_time", time);
        }

        host_object.insert("host_id", QString::number(info.host_id));

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
