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

#include "client/desktop/management/router_users_widget.h"

#include <QDataStream>
#include <QEvent>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QIODevice>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>

#include "base/logging.h"
#include "base/peer/user.h"
#include "client/router_controller.h"
#include "client/desktop/management/router_user_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_users_widget.h"

//--------------------------------------------------------------------------------------------------
RouterUsersWidget::RouterUsersWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_USERS, parent),
      ui(std::make_unique<Ui::RouterUsersWidget>()),
      status_users_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    model_ = new UserListModel(this);
    ui->tree_users->setModel(model_);

    // Turned on again after the model is set: the view wires the header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_users->setSortingEnabled(true);

    ui->tree_users->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users, &QWidget::customContextMenuRequested,
            this, &RouterUsersWidget::onUserContextMenu);

    ui->tree_users->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_users->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterUsersWidget::onHeaderContextMenu);

    connect(ui->tree_users->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterUsersWidget::sig_currentChanged);
    connect(ui->tree_users, &QAbstractItemView::activated,
            this, [this](const QModelIndex&) { onModifyUser(); });

    ui->combo_users_page_size->addItem("25", QVariant::fromValue<qint64>(25));
    ui->combo_users_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_users_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_users_page_size->setCurrentIndex(1);
    users_page_.setPageSize(ui->combo_users_page_size->currentData().toLongLong());

    connect(ui->combo_users_page_size, &QComboBox::currentIndexChanged,
            this, &RouterUsersWidget::onUsersPageSizeChanged);
    connect(ui->combo_users_page, &QComboBox::currentIndexChanged,
            this, &RouterUsersWidget::onUsersPageChanged);
    connect(ui->button_users_prev, &QAbstractButton::clicked,
            this, &RouterUsersWidget::onUsersPrevClicked);
    connect(ui->button_users_next, &QAbstractButton::clicked,
            this, &RouterUsersWidget::onUsersNextClicked);

    updateUsersPagination();

    ui->tree_users->installEventFilter(this);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_usersChanged, this, [this](qint64 router_id)
    {
        if (router_id == router_id_)
            fetchUsers();
    });
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
        {
            model_->clear();
            users_page_.clear();
            updateUsersPagination();
            updateStatusLabel();
        }
    });
}

//--------------------------------------------------------------------------------------------------
RouterUsersWidget::~RouterUsersWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::showRouter(qint64 router_id)
{
    router_id_ = router_id;

    model_->clear();
    users_page_.clear();
    updateUsersPagination();
    updateStatusLabel();
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
bool RouterUsersWidget::hasSelectedUser() const
{
    return currentUser() != nullptr;
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
    const RouterUser* user = currentUser();
    if (!user)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    RouterUserDialog dialog(router_id_, user->entry_id, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onDeleteUser()
{
    const RouterUser* user = currentUser();
    if (!user)
    {
        LOG(INFO) << "No selected user";
        return;
    }

    qint64 entry_id = user->entry_id;
    if (entry_id == 1)
    {
        LOG(INFO) << "Unable to delete built-in user";
        MsgBox::warning(this, tr("You cannot delete a built-in user."));
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to delete user \"%1\"?").arg(user->name))
        != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Delete user rejected by user";
        return;
    }

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    LOG(INFO) << "[ACTION] Delete user accepted by user";
    session->deleteUser(entry_id, { this, &RouterUsersWidget::onUserResultReceived });
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
    if (list.error_code() != proto::router::kErrorOk)
    {
        // An error reply carries no list; treating it as an empty one would remove every user
        // from the tree. Keep what is shown - the next notification triggers another fetch.
        LOG(ERROR) << "Unable to get the list of the users:" << list.error_code();
        if (model_->rowCount() == 0 && !load_error_shown_)
        {
            // With nothing loaded yet an empty tree would silently pass for "no users".
            load_error_shown_ = true;
            MsgBox::warning(this, tr("Failed to get list of users."));
        }
        return;
    }

    load_error_shown_ = false;

    const RouterUser* selected = currentUser();
    const qint64 selected_entry_id = selected ? selected->entry_id : 0;

    model_->setUsers(list);

    // The page is replaced whole, so the row the user was on has to be found again by the record it
    // was showing.
    const int selected_row = model_->rowOf(selected_entry_id);
    if (selected_row >= 0)
        ui->tree_users->setCurrentIndex(model_->index(selected_row, 0));

    const bool page_moved = users_page_.setTotalCount(list.total_count());
    updateUsersPagination();

    // The page the list was fetched for is gone, so the tree was just emptied. Nothing else asks
    // for the page it was moved to, and the user would be left looking at nothing.
    if (page_moved)
        fetchUsers();

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUserResultReceived(const proto::router::UserResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
    }

    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUserContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_users->indexAt(pos);
    if (index.isValid())
        ui->tree_users->setCurrentIndex(index);

    User user;
    if (const RouterUser* selected = model_->userAt(index.row()))
        user = *selected;

    emit sig_userContextMenu(user, ui->tree_users->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_users->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUsersPageSizeChanged(int /* index */)
{
    users_page_.setPageSize(ui->combo_users_page_size->currentData().toLongLong());
    users_page_.setCurrentPage(0);
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUsersPageChanged(int index)
{
    if (index < 0 || index == users_page_.currentPage())
        return;

    users_page_.setCurrentPage(index);
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUsersPrevClicked()
{
    if (users_page_.currentPage() <= 0)
        return;

    users_page_.setCurrentPage(users_page_.currentPage() - 1);
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::onUsersNextClicked()
{
    if (users_page_.currentPage() >= users_page_.pageCount() - 1)
        return;

    users_page_.setCurrentPage(users_page_.currentPage() + 1);
    fetchUsers();
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::fetchUsers()
{
    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    if (session->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    session->listUsers(users_page_.offset(), users_page_.pageSize(),
                       { this, &RouterUsersWidget::onUserListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::updateUsersPagination()
{
    const qint64 total_pages = users_page_.pageCount();

    QSignalBlocker blocker(ui->combo_users_page);
    ui->combo_users_page->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        ui->combo_users_page->addItem(QString::number(i));
    ui->combo_users_page->setCurrentIndex(static_cast<int>(users_page_.currentPage()));

    ui->combo_users_page->setEnabled(total_pages > 1);
    ui->button_users_prev->setEnabled(users_page_.currentPage() > 0);
    ui->button_users_next->setEnabled(users_page_.currentPage() < total_pages - 1);
}

//--------------------------------------------------------------------------------------------------
void RouterUsersWidget::updateStatusLabel()
{
    status_users_label_->setText(tr("%n user(s)", "", model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
const RouterUser* RouterUsersWidget::currentUser() const
{
    return model_->userAt(ui->tree_users->currentIndex().row());
}
