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

#include "client/ui/computers_tab/local_group_dialog.h"

#include "base/logging.h"
#include "client/local_data.h"
#include "client/local_database.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

namespace client {

//--------------------------------------------------------------------------------------------------
LocalGroupDialog::LocalGroupDialog(qint64 group_id, qint64 parent_id, QWidget* parent)
    : QDialog(parent),
      group_id_(group_id)
{
    LOG(INFO) << "Ctor";

    ui.setupUi(this);

    if (group_id_ != -1)
    {
        setWindowTitle(tr("Edit Group"));

        std::optional<GroupData> group = LocalDatabase::instance().findGroup(group_id_);
        if (group.has_value())
        {
            ui.edit_name->setText(group->name);
            ui.edit_comment->setPlainText(group->comment);
            parent_id_ = group->parent_id;
        }
        else
        {
            LOG(ERROR) << "Unable to find group with id" << group_id_;
        }
    }
    else
    {
        setWindowTitle(tr("Add Group"));
        parent_id_ = parent_id;
    }

    connect(ui.button_box, &QDialogButtonBox::clicked, this, &LocalGroupDialog::onButtonBoxClicked);
    ui.edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
LocalGroupDialog::~LocalGroupDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalGroupDialog::onButtonBoxClicked(QAbstractButton* button)
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

    GroupData group;
    group.id = group_id_;
    group.parent_id = parent_id_;
    group.name = ui.edit_name->text();
    group.comment = ui.edit_comment->toPlainText();

    LocalDatabase& db = LocalDatabase::instance();

    if (group_id_ == -1)
    {
        if (!db.addGroup(group))
        {
            QMessageBox::warning(this, tr("Warning"), tr("Unable to add group"));
            LOG(INFO) << "Unable to add group to database";
            return;
        }
    }
    else
    {
        if (!db.modifyGroup(group))
        {
            QMessageBox::warning(this, tr("Warning"), tr("Unable to modify group"));
            LOG(INFO) << "Unable to modify group in database";
            return;
        }
    }

    accept();
}

} // namespace client
