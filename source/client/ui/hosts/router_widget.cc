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

#include <QCollator>
#include <QDateTime>
#include <QIODevice>

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
    connect(connection_, &RouterConnection::sig_userListReceived,
            this, &RouterWidget::onUserListReceived, Qt::QueuedConnection);
    connect(connection_, &RouterConnection::sig_userResultReceived,
            this, &RouterWidget::onUserResultReceived, Qt::QueuedConnection);

    connect(this, &RouterWidget::sig_relayListRequest,
            connection_, &RouterConnection::onRelayListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_userListRequest,
            connection_, &RouterConnection::onUserListRequest, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_addUser,
            connection_, &RouterConnection::onAddUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_modifyUser,
            connection_, &RouterConnection::onModifyUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_deleteUser,
            connection_, &RouterConnection::onDeleteUser, Qt::QueuedConnection);
    connect(this, &RouterWidget::sig_updateConfig,
            connection_, &RouterConnection::onUpdateConfig, Qt::QueuedConnection);

    connect(ui.tab, &QTabWidget::currentChanged, this, &RouterWidget::onTabChanged);
    connect(ui.tree_users, &QTreeWidget::itemSelectionChanged,
            this, &RouterWidget::onCurrentUserChanged);

    ui.tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &RouterWidget::onUserContextMenuRequested);
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
int RouterWidget::itemCount() const
{
    return 0;
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

    stream >> relays_columns_state;
    stream >> splitter_state;
    stream >> peers_columns_state;
    stream >> users_columns_state;

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
void RouterWidget::onTabChanged(int index)
{
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
void RouterWidget::onStatusChanged(const QUuid& uuid, RouterConnection::Status status)
{
    if (status == RouterConnection::Status::ONLINE)
    {
        onUpdateRelayList();
        onUpdateUserList();

        ui.tree_relays->setEnabled(true);
        ui.tree_peers->setEnabled(true);
        ui.tree_users->setEnabled(true);
    }
    else
    {
        ui.tree_relays->setEnabled(false);
        ui.tree_peers->setEnabled(false);
        ui.tree_users->setEnabled(false);

        ui.tree_relays->clear();
        ui.tree_peers->clear();
        ui.tree_users->clear();
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

} // namespace client
