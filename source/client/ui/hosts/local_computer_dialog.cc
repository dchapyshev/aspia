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
#include "base/net/address.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/local_data.h"
#include "client/local_database.h"
#include "client/ui/hosts/group_combo_box.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>

namespace client {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

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

    ui.combo_group->loadGroups(tr("Local"));
    ui.combo_group->selectGroup(group_id_);

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

    QString name = ui.edit_name->text();
    if (name.length() < kMinNameLength)
    {
        QMessageBox::warning(this, tr("Warning"), tr("Name cannot be empty."));
        ui.edit_name->setFocus();
        return;
    }

    if (name.length() > kMaxNameLength)
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Too long name. The maximum length of the name is %n characters.",
               "", kMaxNameLength));
        ui.edit_name->setFocus();
        ui.edit_name->selectAll();
        return;
    }

    base::Address address = base::Address::fromString(ui.edit_address->text(), DEFAULT_HOST_TCP_PORT);
    if (!address.isValid())
    {
        QMessageBox::warning(this, tr("Warning"), tr("An invalid computer address was entered."));
        ui.edit_address->setFocus();
        ui.edit_address->selectAll();
        return;
    }

    QString username = ui.edit_username->text();
    if (!username.isEmpty() && !base::User::isValidUserName(username))
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("The user name can not be empty and can contain only"
               " alphabet characters, numbers and \"_\", \"-\", \".\" characters."));
        ui.edit_username->setFocus();
        ui.edit_username->selectAll();
        return;
    }

    QString comment = ui.edit_comment->toPlainText();
    if (comment.length() > kMaxCommentLength)
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Too long comment. The maximum length of the comment is %n characters.",
               "", kMaxCommentLength));
        ui.edit_comment->setFocus();
        ui.edit_comment->selectAll();
        return;
    }

    qint64 group_id = ui.combo_group->currentGroupId();

    QList<ComputerData> computers = LocalDatabase::instance().computerList(group_id);
    for (const ComputerData& existing : std::as_const(computers))
    {
        if (existing.id != computer_id_ && existing.name == name)
        {
            QMessageBox::warning(this, tr("Warning"),
                tr("A computer with this name already exists in the selected group."));
            ui.edit_name->setFocus();
            return;
        }
    }

    ComputerData computer;
    computer.id = computer_id_;
    computer.group_id = group_id;
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


} // namespace client
