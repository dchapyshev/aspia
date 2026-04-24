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

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QStatusBar>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/user.h"
#include "client/ui/hosts/router_user_dialog.h"
#include "common/ui/msg_box.h"
#include "proto/router_admin.h"

namespace client {

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

        setText(3, QString("%1.%2.%3.%4")
            .arg(version.major()).arg(version.minor()).arg(version.patch()).arg(version.revision()));
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
    explicit PeerTreeItem(const proto::router::PeerConnection& connection)
    {
        updateItem(connection);

        setIcon(0, QIcon(":/img/user.svg"));
        setText(0, QString::fromStdString(conn.client_user_name()));
        setText(1, QString::number(conn.host_id()));
        setText(2, QString::fromStdString(conn.host_address()));
        setText(3, QString::fromStdString(conn.client_address()));
    }

    void updateItem(const proto::router::PeerConnection& connection)
    {
        conn = connection;
        setText(4, RouterWidget::sizeToString(conn.bytes_transferred()));
        setText(5, RouterWidget::delayToString(conn.duration()));
        setText(6, RouterWidget::delayToString(conn.idle_time()));
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

    proto::router::PeerConnection conn;

private:
    Q_DISABLE_COPY_MOVE(PeerTreeItem)
};

class HostTreeItem final : public QTreeWidgetItem
{
public:
    explicit HostTreeItem(const proto::router::HostInfo& info)
    {
        updateItem(info);

        QString time = QLocale::system().toString(
            QDateTime::fromSecsSinceEpoch(info.timepoint()), QLocale::ShortFormat);

        setIcon(0, QIcon(":/img/computer.svg"));
        setText(0, QString::fromStdString(info.computer_name()));
        setText(1, QString::fromStdString(info.ip_address()));
        setText(2, time);

        const proto::peer::Version& version = info.version();

        setText(4, QString("%1.%2.%3.%4")
            .arg(version.major()).arg(version.minor()).arg(version.patch()).arg(version.revision()));
        setText(5, QString::fromStdString(info.architecture()));
        setText(6, QString::fromStdString(info.os_name()));
    }

    void updateItem(const proto::router::HostInfo& updated_info)
    {
        info = updated_info;
        setText(3, QString::number(info.host_id()));
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
            const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);
            return info.timepoint() < other_item->info.timepoint();
        }

        return QTreeWidgetItem::operator<(other);
    }

    proto::router::HostInfo info;

private:
    Q_DISABLE_COPY_MOVE(HostTreeItem)
};

class UserTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(UserTreeItem)

public:
    explicit UserTreeItem(const proto::router::User& user)
        : user(base::User::parseFrom(user))
    {
        setText(0, QString::fromStdString(user.name()));
        setText(1, user.flags() & base::User::ENABLED ? tr("Yes") : tr("No"));
        setText(2, sessionsToString(user.sessions()));

        if (user.flags() & base::User::ENABLED)
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

    base::User user;

private:
    Q_DISABLE_COPY_MOVE(UserTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWidget::RouterWidget(const RouterConfig& config, QWidget* parent)
    : ContentWidget(Type::ROUTER, parent),
      uuid_(config.uuid)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    connection_ = new RouterConnection(config);
    connection_->moveToThread(base::GuiApplication::ioThread());

    connect(connection_, &RouterConnection::sig_statusChanged,
            this, &RouterWidget::onStatusChanged, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_relayListReceived,
            this, &RouterWidget::onRelayListReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_hostListReceived,
            this, &RouterWidget::onHostListReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_userListReceived,
            this, &RouterWidget::onUserListReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_userResultReceived,
            this, &RouterWidget::onUserResultReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_hostResultReceived,
            this, &RouterWidget::onHostResultReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_relayResultReceived,
            this, &RouterWidget::onRelayResultReceived, Qt::QueuedConnection);

    connect(this, &RouterWidget::sig_relayListRequest,
            connection_, &RouterConnection::onRelayListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_hostListRequest,
            connection_, &RouterConnection::onHostListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_userListRequest,
            connection_, &RouterConnection::onUserListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_addUser,
            connection_, &RouterConnection::onAddUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_modifyUser,
            connection_, &RouterConnection::onModifyUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_deleteUser,
            connection_, &RouterConnection::onDeleteUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectHost,
            connection_, &RouterConnection::onDisconnectHost, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_removeHost,
            connection_, &RouterConnection::onRemoveHost, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_disconnectRelay,
            connection_, &RouterConnection::onDisconnectRelay, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_updateConfig,
            connection_, &RouterConnection::onUpdateConfig, Qt::QueuedConnection);

    connect(ui.tab, &QTabWidget::currentChanged, this, &RouterWidget::onTabChanged);
    connect(ui.tree_users, &QTreeWidget::itemSelectionChanged,
            this, &RouterWidget::onCurrentUserChanged);
    connect(ui.tree_relays, &QTreeWidget::itemSelectionChanged,
            this, &RouterWidget::onCurrentRelayChanged);
    connect(ui.tree_hosts, &QTreeWidget::itemSelectionChanged,
            this, &RouterWidget::onCurrentHostChanged);

    ui.tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onUserContextMenuRequested);

    ui.tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.tree_hosts, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onHostContextMenuRequested);

    ui.tree_relays->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.tree_relays, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onRelayContextMenuRequested);
}

//--------------------------------------------------------------------------------------------------
RouterWidget::~RouterWidget()
{
    LOG(INFO) << "Dtor";

    if (connection_)
    {
        connection_->disconnect();
        connection_->deleteLater();
        connection_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
const QUuid& RouterWidget::uuid() const
{
    return uuid_;
}

//--------------------------------------------------------------------------------------------------
RouterConnection::Status RouterWidget::status() const
{
    return status_;
}

//--------------------------------------------------------------------------------------------------
RouterWidget::TabType RouterWidget::currentTabType() const
{
    return static_cast<TabType>(ui.tab->currentIndex());
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedUser() const
{
    return ui.tree_users->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
bool RouterWidget::hasSelectedHost() const
{
    return ui.tree_hosts->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::hostCount() const
{
    return ui.tree_hosts->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentHostRow()
{
    QTreeWidgetItem* item = ui.tree_hosts->currentItem();
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
    QTreeWidgetItem* item = ui.tree_hosts->currentItem();
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
    return ui.tree_relays->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
int RouterWidget::relayCount() const
{
    return ui.tree_relays->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::copyCurrentRelayRow()
{
    QTreeWidgetItem* item = ui.tree_relays->currentItem();
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
    QTreeWidgetItem* item = ui.tree_relays->currentItem();
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
    if (connection_)
    {
        QMetaObject::invokeMethod(
            connection_, &RouterConnection::onConnectToRouter, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::disconnectFromRouter()
{
    if (connection_)
    {
        QMetaObject::invokeMethod(
            connection_, &RouterConnection::onDisconnectFromRouter, Qt::QueuedConnection);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateConfig(const RouterConfig& config)
{
    emit sig_updateConfig(config);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        stream << ui.tree_relays->header()->saveState();
        stream << ui.splitter->saveState();
        stream << ui.tree_peers->header()->saveState();
        stream << ui.tree_users->header()->saveState();
        stream << ui.tree_hosts->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_15);

    QByteArray relays_columns_state;
    QByteArray splitter_state;
    QByteArray peers_columns_state;
    QByteArray users_columns_state;
    QByteArray hosts_columns_state;

    stream >> relays_columns_state;
    stream >> splitter_state;
    stream >> peers_columns_state;
    stream >> users_columns_state;
    stream >> hosts_columns_state;

    if (!relays_columns_state.isEmpty())
    {
        ui.tree_relays->header()->restoreState(relays_columns_state);
        ui.tree_relays->header()->setSectionsClickable(true);
        ui.tree_relays->header()->setSortIndicatorShown(true);
    }

    if (!splitter_state.isEmpty())
    {
        ui.splitter->restoreState(splitter_state);
    }
    else
    {
        int side_size = height() / 2;

        QList<int> sizes;
        sizes.emplace_back(side_size);
        sizes.emplace_back(side_size);
        ui.splitter->setSizes(sizes);
    }

    if (!peers_columns_state.isEmpty())
    {
        ui.tree_peers->header()->restoreState(peers_columns_state);
        ui.tree_peers->header()->setSectionsClickable(true);
        ui.tree_peers->header()->setSortIndicatorShown(true);
    }

    if (!users_columns_state.isEmpty())
    {
        ui.tree_users->header()->restoreState(users_columns_state);
        ui.tree_users->header()->setSectionsClickable(true);
        ui.tree_users->header()->setSortIndicatorShown(true);
    }

    if (!hosts_columns_state.isEmpty())
    {
        ui.tree_hosts->header()->restoreState(hosts_columns_state);
        ui.tree_hosts->header()->setSectionsClickable(true);
        ui.tree_hosts->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::reload()
{
    switch (currentTabType())
    {
        case TabType::HOSTS:
            onUpdateHostList();
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
    return tab == TabType::HOSTS || tab == TabType::RELAYS;
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::save()
{
    switch (currentTabType())
    {
        case TabType::HOSTS:
            saveHostsToFile();
            break;
        case TabType::RELAYS:
            saveRelaysToFile();
            break;
        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::attachStatusBar(QStatusBar* statusbar)
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
void RouterWidget::detachStatusBar(QStatusBar* statusbar)
{
    if (!statusbar || !status_label_)
        return;

    statusbar->removeWidget(status_label_);
    status_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateStatusLabel()
{
    if (!status_label_)
        return;

    switch (currentTabType())
    {
        case TabType::HOSTS:
            status_label_->setText(tr("%n host(s)", "", ui.tree_hosts->topLevelItemCount()));
            break;
        case TabType::RELAYS:
            status_label_->setText(tr("%n relay(s)", "", ui.tree_relays->topLevelItemCount()));
            break;
        case TabType::USERS:
            status_label_->setText(tr("%n user(s)", "", ui.tree_users->topLevelItemCount()));
            break;
        default:
            status_label_->clear();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterWidget::delayToString(quint64 delay)
{
    quint64 days = (delay / 86400);
    quint64 hours = (delay % 86400) / 3600;
    quint64 minutes = ((delay % 86400) % 3600) / 60;
    quint64 seconds = ((delay % 86400) % 3600) % 60;

    QString seconds_string = tr("%n seconds", "", static_cast<int>(seconds));
    QString minutes_string = tr("%n minutes", "", static_cast<int>(minutes));
    QString hours_string = tr("%n hours", "", static_cast<int>(hours));

    if (days)
    {
        QString days_string = tr("%n days", "", static_cast<int>(days));
        return days_string + ' ' + hours_string + ' ' + minutes_string + ' ' + seconds_string;
    }

    if (hours)
        return hours_string + ' ' + minutes_string + ' ' + seconds_string;

    if (minutes)
        return minutes_string + ' ' + seconds_string;

    return seconds_string;
}

//--------------------------------------------------------------------------------------------------
// static
QString RouterWidget::sizeToString(qint64 size)
{
    static const qint64 kKB = 1024LL;
    static const qint64 kMB = kKB * 1024LL;
    static const qint64 kGB = kMB * 1024LL;
    static const qint64 kTB = kGB * 1024LL;

    QString units;
    qint64 divider;

    if (size >= kTB)
        units = tr("TB"), divider = kTB;
    else if (size >= kGB)
        units = tr("GB"), divider = kGB;
    else if (size >= kMB)
        units = tr("MB"), divider = kMB;
    else if (size >= kKB)
        units = tr("kB"), divider = kKB;
    else
        units = tr("B"), divider = 1;

    return QString("%1 %2").arg(double(size) / double(divider), 0, 'g', 4).arg(units);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateRelayList()
{
    emit sig_relayListRequest();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUpdateHostList()
{
    emit sig_hostListRequest();
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
    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui.tree_users->topLevelItem(i));
        names.append(item->user.name);
    }

    RouterUserDialog dialog(base::User(), names, this);
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
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    QStringList names;
    for (int i = 0; i < ui.tree_users->topLevelItemCount(); ++i)
    {
        UserTreeItem* item = static_cast<UserTreeItem*>(ui.tree_users->topLevelItem(i));
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
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    qint64 entry_id = tree_item->user.entry_id;
    if (entry_id == 1)
    {
        LOG(INFO) << "Unable to delete built-in user";
        common::MsgBox::warning(this, tr("You cannot delete a built-in user."));
        return;
    }

    if (common::MsgBox::question(this,
            tr("Are you sure you want to delete user \"%1\"?").arg(tree_item->text(0)))
        != common::MsgBox::Yes)
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
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui.tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    if (common::MsgBox::question(this, tr("Are you sure you want to disconnect host \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.computer_name()))) != common::MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect host accepted by user";
    emit sig_disconnectHost(tree_item->info.entry_id());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectAllHosts()
{
    if (ui.tree_hosts->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Host list is empty";
        return;
    }

    if (common::MsgBox::question(this,
            tr("Are you sure you want to disconnect all hosts?")) != common::MsgBox::Yes)
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
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui.tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    common::MsgBox message_box(this);
    message_box.setWindowTitle(tr("Confirmation"));
    message_box.setText(tr("Deleting a host will result in all its configuration for connecting "
                           "to the router being deleted. This operation is irreversible. After "
                           "deleting, the host will no longer connect to the router. Are you sure "
                           "you want to do this?"));
    message_box.setIcon(common::MsgBox::Question);
    message_box.setStandardButtons(common::MsgBox::Yes | common::MsgBox::No);

    QCheckBox* check_box = new QCheckBox(&message_box);
    check_box->setText(tr("Try to uninstall the application (result is not guaranteed)"));
    message_box.setCheckBox(check_box);

    if (message_box.exec() == common::MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Remove host rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Remove host accepted by user";
    emit sig_removeHost(tree_item->info.entry_id(), check_box->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onDisconnectRelay()
{
    RelayTreeItem* tree_item = static_cast<RelayTreeItem*>(ui.tree_relays->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected relay";
        return;
    }

    if (common::MsgBox::question(this, tr("Are you sure you want to disconnect relay \"%1\"?")
        .arg(QString::fromStdString(tree_item->info.ip_address()))) != common::MsgBox::Yes)
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
    if (ui.tree_relays->topLevelItemCount() <= 0)
    {
        LOG(INFO) << "Relay list is empty";
        return;
    }

    if (common::MsgBox::question(this,
            tr("Are you sure you want to disconnect all relays?")) != common::MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all relays rejected by user";
        return;
    }

    LOG(INFO) << "[ACTION] Disconnect all relays accepted by user";
    emit sig_disconnectRelay(-1);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onTabChanged(int index)
{
    updateStatusLabel();
    emit sig_currentTabTypeChanged(uuid_, static_cast<TabType>(index));
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentUserChanged()
{
    emit sig_currentUserChanged(uuid_);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui.tree_users->itemAt(pos);
    if (item)
        ui.tree_users->setCurrentItem(item);

    base::User user;
    if (item)
        user = static_cast<UserTreeItem*>(item)->user;

    QPoint global_pos = ui.tree_users->viewport()->mapToGlobal(pos);
    emit sig_userContextMenu(uuid_, user, global_pos);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui.tree_hosts->itemAt(pos);
    if (item)
        ui.tree_hosts->setCurrentItem(item);

    const int column = ui.tree_hosts->indexAt(pos).column();
    const QPoint global_pos = ui.tree_hosts->viewport()->mapToGlobal(pos);
    emit sig_hostContextMenu(uuid_, global_pos, column);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui.tree_relays->itemAt(pos);
    if (item)
        ui.tree_relays->setCurrentItem(item);

    const int column = ui.tree_relays->indexAt(pos).column();
    const QPoint global_pos = ui.tree_relays->viewport()->mapToGlobal(pos);
    emit sig_relayContextMenu(uuid_, global_pos, column);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onStatusChanged(const QUuid& uuid, RouterConnection::Status status)
{
    status_ = status;

    if (status == RouterConnection::Status::ONLINE)
    {
        onUpdateRelayList();
        onUpdateHostList();
        onUpdateUserList();

        ui.tree_relays->setEnabled(true);
        ui.tree_peers->setEnabled(true);
        ui.tree_hosts->setEnabled(true);
        ui.tree_users->setEnabled(true);
    }
    else
    {
        ui.tree_relays->setEnabled(false);
        ui.tree_peers->setEnabled(false);
        ui.tree_hosts->setEnabled(false);
        ui.tree_users->setEnabled(false);

        ui.tree_relays->clear();
        ui.tree_peers->clear();
        ui.tree_hosts->clear();
        ui.tree_users->clear();

        updateStatusLabel();
    }

    emit sig_statusChanged(uuid, status);
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
    for (int i = ui.tree_relays->topLevelItemCount() - 1; i >= 0; --i)
    {
        RelayTreeItem* item = static_cast<RelayTreeItem*>(ui.tree_relays->topLevelItem(i));

        if (!has_with_id(relays, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < relays.relay_size(); ++i)
    {
        const proto::router::RelayInfo& info = relays.relay(i);
        bool found = false;

        for (int j = 0; j < ui.tree_relays->topLevelItemCount(); ++j)
        {
            RelayTreeItem* item = static_cast<RelayTreeItem*>(ui.tree_relays->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui.tree_relays->addTopLevelItem(new RelayTreeItem(info));
    }

    updateRelayStatistics();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostListReceived(const proto::router::HostList& hosts)
{
    auto has_with_id = [](const proto::router::HostList& hosts, qint64 entry_id)
    {
        for (int i = 0; i < hosts.host_size(); ++i)
        {
            if (hosts.host(i).entry_id() == entry_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui.tree_hosts->topLevelItemCount() - 1; i >= 0; --i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui.tree_hosts->topLevelItem(i));

        if (!has_with_id(hosts, item->info.entry_id()))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < hosts.host_size(); ++i)
    {
        const proto::router::HostInfo& info = hosts.host(i);
        bool found = false;

        for (int j = 0; j < ui.tree_hosts->topLevelItemCount(); ++j)
        {
            HostTreeItem* item = static_cast<HostTreeItem*>(ui.tree_hosts->topLevelItem(j));
            if (item->info.entry_id() == info.entry_id())
            {
                item->updateItem(info);
                found = true;
                break;
            }
        }

        if (!found)
            ui.tree_hosts->addTopLevelItem(new HostTreeItem(info));
    }

    emit sig_currentHostChanged(uuid_);
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserListReceived(const proto::router::UserList& list)
{
    QTreeWidget* tree_users = ui.tree_users;

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
        emit sig_currentUserChanged(uuid_);

    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onUserResultReceived(const proto::router::UserResult& result)
{
    if (result.error_code() != proto::router::UserResult::SUCCESS)
    {
        const char* message;

        switch (result.error_code())
        {
            case proto::router::UserResult::INTERNAL_ERROR:
                message = QT_TR_NOOP("Unknown internal error.");
                break;
            case proto::router::UserResult::INVALID_DATA:
                message = QT_TR_NOOP("Invalid data was passed.");
                break;
            case proto::router::UserResult::ALREADY_EXISTS:
                message = QT_TR_NOOP("A user with the specified name already exists.");
                break;
            default:
                message = QT_TR_NOOP("Unknown error type.");
                break;
        }

        common::MsgBox::warning(this, tr(message));
    }

    onUpdateUserList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onHostResultReceived(const proto::router::HostResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != "ok")
    {
        const char* message;

        if (error_code == "invalid_request")
            message = QT_TR_NOOP("Invalid host request.");
        else if (error_code == "internal_error")
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == "invalid_entry_id")
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        common::MsgBox::warning(this, tr(message));
    }

    onUpdateHostList();
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onRelayResultReceived(const proto::router::RelayResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != "ok")
    {
        const char* message;

        if (error_code == "invalid_request")
            message = QT_TR_NOOP("Invalid relay request.");
        else if (error_code == "internal_error")
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == "invalid_entry_id")
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        common::MsgBox::warning(this, tr(message));
    }

    onUpdateRelayList();
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
        common::MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui.tree_hosts->topLevelItemCount(); ++i)
    {
        const proto::router::HostInfo& info =
            static_cast<HostTreeItem*>(ui.tree_hosts->topLevelItem(i))->info;

        QJsonObject host_object;

        host_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        host_object.insert("operating_system", QString::fromStdString(info.os_name()));
        host_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        host_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3.%4")
            .arg(info.version().major()).arg(info.version().minor())
            .arg(info.version().patch()).arg(info.version().revision());
        host_object.insert("version", version);

        QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
            info.timepoint()), QLocale::ShortFormat);
        host_object.insert("connect_time", time);

        host_object.insert("host_id", QString::number(info.host_id()));

        root_array.append(host_object);
    }

    QJsonObject root_object;
    root_object.insert("hosts", root_array);

    qint64 written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        common::MsgBox::warning(this, tr("Unable to write file."));
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
        common::MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui.tree_relays->topLevelItemCount(); ++i)
    {
        const proto::router::RelayInfo& info =
            static_cast<RelayTreeItem*>(ui.tree_relays->topLevelItem(i))->info;

        QJsonObject relay_object;

        relay_object.insert("computer_name", QString::fromStdString(info.computer_name()));
        relay_object.insert("operating_system", QString::fromStdString(info.os_name()));
        relay_object.insert("ip_address", QString::fromStdString(info.ip_address()));
        relay_object.insert("architecture", QString::fromStdString(info.architecture()));

        QString version = QString("%1.%2.%3.%4")
            .arg(info.version().major()).arg(info.version().minor())
            .arg(info.version().patch()).arg(info.version().revision());
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
                const proto::router::PeerConnection& conn = stats.peer(j);

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
        common::MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentRelayChanged()
{
    ui.tree_peers->clear();
    updateRelayStatistics();
    emit sig_currentRelayChanged(uuid_);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::onCurrentHostChanged()
{
    emit sig_currentHostChanged(uuid_);
}

//--------------------------------------------------------------------------------------------------
void RouterWidget::updateRelayStatistics()
{
    RelayTreeItem* item = static_cast<RelayTreeItem*>(ui.tree_relays->currentItem());
    if (!item)
    {
        ui.tree_peers->setEnabled(false);
        return;
    }

    ui.tree_peers->setEnabled(true);

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
    for (int i = ui.tree_peers->topLevelItemCount() - 1; i >= 0; --i)
    {
        PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui.tree_peers->topLevelItem(i));
        if (!has_with_id(stats, peer_item->conn.session_id()))
            delete peer_item;
    }

    // Adding and updating elements in the UI.
    for (int i = 0; i < stats.peer_size(); ++i)
    {
        const proto::router::PeerConnection& connection = stats.peer(i);
        bool found = false;

        for (int j = 0; j < ui.tree_peers->topLevelItemCount(); ++j)
        {
            PeerTreeItem* peer_item = static_cast<PeerTreeItem*>(ui.tree_peers->topLevelItem(j));
            if (peer_item->conn.session_id() == connection.session_id())
            {
                peer_item->updateItem(connection);
                found = true;
                break;
            }
        }

        if (!found)
            ui.tree_peers->addTopLevelItem(new PeerTreeItem(connection));
    }
}

} // namespace client
