//
// PROJECT:         Aspia
// FILE:            host/ui/user_list_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/ui/user_list_dialog.h"

#include <QMenu>
#include <QMessageBox>

#include "host/ui/user_dialog.h"
#include "host/ui/user_tree_item.h"
#include "host/host_settings.h"

namespace aspia {

UserListDialog::UserListDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.tree_users, &QTreeWidget::customContextMenuRequested,
            this, &UserListDialog::onContextMenu);

    connect(ui.tree_users, &QTreeWidget::currentItemChanged,
            this, &UserListDialog::onCurrentItemChanged);

    connect(ui.tree_users, &QTreeWidget::itemDoubleClicked,
            [this](QTreeWidgetItem* /* item */, int /* column */)
    {
        onModifyUser();
    });

    connect(ui.action_add, &QAction::triggered, this, &UserListDialog::onAddUser);
    connect(ui.action_modify, &QAction::triggered, this, &UserListDialog::onModifyUser);
    connect(ui.action_delete, &QAction::triggered, this, &UserListDialog::onDeleteUser);
    connect(ui.button_add, &QPushButton::pressed, this, &UserListDialog::onAddUser);
    connect(ui.button_modify, &QPushButton::pressed, this, &UserListDialog::onModifyUser);
    connect(ui.button_delete, &QPushButton::pressed, this, &UserListDialog::onDeleteUser);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &UserListDialog::onButtonBoxClicked);

    user_list_ = HostSettings().userList();
    reloadUserList();
}

void UserListDialog::onContextMenu(const QPoint& point)
{
    QMenu menu;

    QTreeWidgetItem* current_item = ui.tree_users->itemAt(point);
    if (current_item)
    {
        ui.tree_users->setCurrentItem(current_item);

        menu.addAction(ui.action_modify);
        menu.addAction(ui.action_delete);
        menu.addSeparator();
    }

    menu.addAction(ui.action_add);
    menu.exec(ui.tree_users->mapToGlobal(point));
}

void UserListDialog::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    ui.button_modify->setEnabled(true);
    ui.button_delete->setEnabled(true);

    ui.action_modify->setEnabled(true);
    ui.action_delete->setEnabled(true);
}

void UserListDialog::onAddUser()
{
    User user;
    user.setFlags(User::FLAG_ENABLED);

    if (UserDialog(&user_list_, &user, this).exec() == QDialog::Accepted)
    {
        user_list_.push_back(user);
        reloadUserList();
    }
}

void UserListDialog::onModifyUser()
{
    UserTreeItem* user_item = reinterpret_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (UserDialog(&user_list_, user_item->user(), this).exec() == QDialog::Accepted)
        reloadUserList();
}

void UserListDialog::onDeleteUser()
{
    UserTreeItem* user_item = reinterpret_cast<UserTreeItem*>(ui.tree_users->currentItem());
    if (!user_item)
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete user \"%1\"?")
                                  .arg(user_item->user()->name()),
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        user_list_.removeAt(user_item->userIndex());
        reloadUserList();
    }
}

void UserListDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        if (!HostSettings().setUserList(user_list_))
        {
            QString message =
                tr("The user list could not be written. Make sure that you have sufficient rights to write.");

            QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
            return;
        }

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void UserListDialog::reloadUserList()
{
    for (int i = ui.tree_users->topLevelItemCount() - 1; i >= 0; --i)
        delete ui.tree_users->takeTopLevelItem(i);

    for (int i = 0; i < user_list_.size(); ++i)
        ui.tree_users->addTopLevelItem(new UserTreeItem(i, &user_list_[i]));

    ui.button_modify->setEnabled(false);
    ui.button_delete->setEnabled(false);

    ui.action_modify->setEnabled(false);
    ui.action_delete->setEnabled(false);
}

} // namespace aspia
