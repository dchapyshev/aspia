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

#include "client/desktop/hosts/router_users_widget.h"

#include <QCollator>
#include <QDataStream>
#include <QEvent>
#include <QHeaderView>
#include <QIODevice>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>

#include "base/logging.h"
#include "base/peer/router_user.h"
#include "base/peer/user.h"
#include "client/router.h"
#include "client/desktop/hosts/router_user_dialog.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_users_widget.h"

namespace {

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

} // namespace

//--------------------------------------------------------------------------------------------------
RouterUsersWidget::RouterUsersWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_USERS, parent),
      ui(std::make_unique<Ui::RouterUsersWidget>()),
      status_users_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users, &QTreeWidget::customContextMenuRequested,
            this, &RouterUsersWidget::onUserContextMenu);

    ui->tree_users->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterUsersWidget::onHeaderContextMenu);

    connect(ui->tree_users, &QTreeWidget::itemSelectionChanged,
            this, &RouterUsersWidget::sig_currentChanged);
    connect(ui->tree_users, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem*, int) { onModifyUser(); });

    ui->tree_users->installEventFilter(this);
}

//--------------------------------------------------------------------------------------------------
RouterUsersWidget::~RouterUsersWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::showRouter(qint64 router_id)
{
    if (router_id_ != router_id)
    {
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, nullptr, this, nullptr);

        if (Router* curr = Router::instance(router_id))
        {
            connect(curr, &Router::sig_usersChanged, this, &RouterUsersWidget::fetchUsers);
            connect(curr, &Router::sig_statusChanged, this, [this](qint64, Router::Status status)
            {
                if (status != Router::Status::ONLINE)
                {
                    ui->tree_users->clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;

    ui->tree_users->clear();
    updateStatusLabel();
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
bool RouterUsersWidget::hasSelectedUser() const
{
    return ui->tree_users->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterUsersWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_users->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_users->header()->restoreState(columns_state);
        ui->tree_users->header()->setSectionsClickable(true);
        ui->tree_users->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::reload()
{
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_users_label_);
    status_users_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_users_label_);
    status_users_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onAddUser()
{
    RouterUserDialog dialog(router_id_, 0, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onModifyUser()
{
    UserTreeItem* tree_item = static_cast<UserTreeItem*>(ui->tree_users->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    RouterUserDialog dialog(router_id_, tree_item->user.entry_id, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onDeleteUser()
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

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Delete user accepted by user";
    router->deleteUser(entry_id, this, &RouterUsersWidget::onUserResultReceived);
}

//--------------------------------------------------------------------------------------------------
bool RouterUsersWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tree_users && event->type() == QEvent::KeyPress)
    {
        switch (static_cast<QKeyEvent*>(event)->key())
        {
            case Qt::Key_Insert:
                onAddUser();
                return true;

            case Qt::Key_Delete:
                onDeleteUser();
                return true;

            case Qt::Key_F2:
                onModifyUser();
                return true;

            default:
                break;
        }
    }

    return ContentWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUserListReceived(const proto::router::UserList& list)
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

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUserResultReceived(const proto::router::UserResult& result)
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

    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUserContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_users->itemAt(pos);
    if (item)
        ui->tree_users->setCurrentItem(item);

    User user;
    if (item)
        user = static_cast<UserTreeItem*>(item)->user;

    emit sig_userContextMenu(user, ui->tree_users->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_users->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_users->headerItem()->text(i), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::fetchUsers()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    router->listUsers(this, &RouterUsersWidget::onUserListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::updateStatusLabel()
{
    status_users_label_->setText(tr("%n user(s)", "", ui->tree_users->topLevelItemCount()));
}
