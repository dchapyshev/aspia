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

#include "client/ui/hosts/local_computer_dialog.h"

#include "base/logging.h"
#include "client/local_data.h"
#include "client/local_database.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTreeView>

namespace client {

namespace {

const int kGroupIdRole = Qt::UserRole + 1;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::LocalComputerDialog(qint64 computer_id, qint64 group_id, QWidget* parent)
    : QDialog(parent),
      computer_id_(computer_id)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    if (computer_id_ != -1)
    {
        setWindowTitle(tr("Edit Computer"));

        std::optional<ComputerData> computer = LocalDatabase::instance().findComputer(computer_id_);
        if (computer.has_value())
        {
            ui.edit_name->setText(computer->name);
            ui.edit_address->setText(computer->address);
            ui.edit_username->setText(computer->username);
            ui.edit_password->setText(computer->password);
            ui.edit_comment->setPlainText(computer->comment);
            group_id_ = computer->group_id;
        }
        else
        {
            LOG(ERROR) << "Unable to find computer with id" << computer_id_;
        }
    }
    else
    {
        setWindowTitle(tr("Add Computer"));
        group_id_ = group_id;
    }

    // Build group tree model.
    QStandardItemModel* model = new QStandardItemModel(this);

    QStandardItem* root = new QStandardItem(QIcon(":/img/folder.svg"), tr("Local"));
    root->setData(static_cast<qint64>(0), kGroupIdRole);
    model->appendRow(root);

    addGroupItems(0, root);

    // Set up tree view for combo box popup.
    QTreeView* tree_view = new QTreeView(this);
    tree_view->setHeaderHidden(true);

    ui.combo_group->setModel(model);
    ui.combo_group->setView(tree_view);

    tree_view->expandAll();

    selectGroup(group_id_);

    connect(ui.button_show_password, &QToolButton::toggled,
            this, &LocalComputerDialog::onShowPasswordButtonToggled);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &LocalComputerDialog::onButtonBoxClicked);

    ui.edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::~LocalComputerDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::onShowPasswordButtonToggled(bool checked)
{
    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    if (ui.edit_name->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Name cannot be empty."));
        ui.edit_name->setFocus();
        return;
    }

    if (ui.edit_address->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Address cannot be empty."));
        ui.edit_address->setFocus();
        return;
    }

    ComputerData computer;
    computer.id = computer_id_;
    computer.group_id = ui.combo_group->currentData(kGroupIdRole).toLongLong();
    computer.name = ui.edit_name->text();
    computer.address = ui.edit_address->text();
    computer.username = ui.edit_username->text();
    computer.password = ui.edit_password->text();
    computer.comment = ui.edit_comment->toPlainText();

    LocalDatabase& db = LocalDatabase::instance();

    if (computer_id_ == -1)
    {
        if (!db.addComputer(computer))
        {
            QMessageBox::warning(this, tr("Warning"), tr("Unable to add computer"));
            LOG(INFO) << "Unable to add computer to database";
            return;
        }
    }
    else
    {
        if (!db.modifyComputer(computer))
        {
            QMessageBox::warning(this, tr("Warning"), tr("Unable to modify computer"));
            LOG(INFO) << "Unable to modify computer in database";
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::addGroupItems(qint64 parent_id, QStandardItem* parent_item)
{
    QIcon folder_icon(":/img/folder.svg");
    QList<GroupData> groups = LocalDatabase::instance().groupList(parent_id);

    for (const GroupData& group : std::as_const(groups))
    {
        QStandardItem* item = new QStandardItem(folder_icon, group.name);
        item->setData(group.id, kGroupIdRole);
        parent_item->appendRow(item);

        addGroupItems(group.id, item);
    }
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::selectGroup(qint64 group_id)
{
    QAbstractItemModel* model = ui.combo_group->model();

    QModelIndexList matches = model->match(
        model->index(0, 0), kGroupIdRole, QVariant::fromValue(group_id), 1,
        Qt::MatchExactly | Qt::MatchRecursive);

    if (!matches.isEmpty())
    {
        QModelIndex index = matches.first();
        ui.combo_group->setRootModelIndex(index.parent());
        ui.combo_group->setCurrentIndex(index.row());
    }
}

} // namespace client
