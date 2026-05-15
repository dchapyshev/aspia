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

#include <QAbstractButton>
#include <QPushButton>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "ui_local_computer_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::LocalComputerDialog(qint64 computer_id, qint64 group_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::LocalComputerDialog>()),
      computer_id_(computer_id)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    ui->combo_router->addItem(QIcon(":/img/connect.svg"), tr("Without Router"), QVariant::fromValue<qint64>(0));

    QList<RouterConfig> routers = Database::instance().routerList();
    for (const RouterConfig& router : std::as_const(routers))
    {
        ui->combo_router->addItem(QIcon(":/img/stack.svg"), router.displayLabel(), QVariant::fromValue(router.routerId()));
    }

    qint64 selected_router_id = 0;

    if (computer_id_ != -1)
    {
        setWindowTitle(tr("Edit Computer"));

        std::optional<ComputerConfig> computer = Database::instance().findComputer(computer_id_);
        if (computer.has_value())
        {
            ui->edit_name->setText(computer->name());
            ui->edit_address->setText(computer->address());
            ui->edit_username->setText(computer->username());
            ui->edit_password->setPassword(computer->password());
            ui->edit_comment->setPlainText(computer->comment());
            group_id_ = computer->groupId();
            selected_router_id = computer->routerId();
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

    if (selected_router_id != 0)
    {
        int found_index = ui->combo_router->findData(QVariant::fromValue(selected_router_id));
        if (found_index < 0)
        {
            LOG(WARNING) << "Computer references missing router id" << selected_router_id;
            ui->combo_router->addItem(QIcon(":/img/high-importance.svg"), tr("<deleted router>"),
                                     QVariant::fromValue(selected_router_id));
            found_index = ui->combo_router->count() - 1;
        }
        ui->combo_router->setCurrentIndex(found_index);
    }

    updateAddressLabel();

    ui->combo_group->loadGroups(tr("Local"));
    ui->combo_group->selectGroup(group_id_);

    connect(ui->button_show_password, &QToolButton::toggled,
            ui->edit_password, &PasswordEdit::setShowPassword);
    connect(ui->combo_router, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalComputerDialog::onRouterChanged);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &LocalComputerDialog::onButtonBoxClicked);

    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
LocalComputerDialog::~LocalComputerDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::onRouterChanged(int /* index */)
{
    updateAddressLabel();
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->button_box->standardButton(button) != QDialogButtonBox::Ok)
    {
        reject();
        return;
    }

    QString name = ui->edit_name->text();
    if (name.length() < kMinNameLength)
    {
        MsgBox::warning(this, tr("Name cannot be empty."));
        ui->edit_name->setFocus();
        return;
    }

    if (name.length() > kMaxNameLength)
    {
        MsgBox::warning(this,
            tr("Too long name. The maximum length of the name is %n characters.",
               "", kMaxNameLength));
        ui->edit_name->setFocus();
        ui->edit_name->selectAll();
        return;
    }

    qint64 router_id = ui->combo_router->currentData().toLongLong();

    if (router_id == 0)
    {
        Address address =
            Address::fromString(ui->edit_address->text(), DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            MsgBox::warning(this, tr("An invalid computer address was entered."));
            ui->edit_address->setFocus();
            ui->edit_address->selectAll();
            return;
        }
    }
    else
    {
        if (!isHostId(ui->edit_address->text()))
        {
            MsgBox::warning(this, tr("An invalid host ID was entered."));
            ui->edit_address->setFocus();
            ui->edit_address->selectAll();
            return;
        }
    }

    QString username = ui->edit_username->text();
    if (!username.isEmpty() && !User::isValidUserName(username))
    {
        MsgBox::warning(this,
            tr("The user name can not be empty and can contain only"
               " alphabet characters, numbers and \"_\", \"-\", \".\" characters."));
        ui->edit_username->setFocus();
        ui->edit_username->selectAll();
        return;
    }

    QString comment = ui->edit_comment->toPlainText();
    if (comment.length() > kMaxCommentLength)
    {
        MsgBox::warning(this,
            tr("Too long comment. The maximum length of the comment is %n characters.",
               "", kMaxCommentLength));
        ui->edit_comment->setFocus();
        ui->edit_comment->selectAll();
        return;
    }

    qint64 group_id = ui->combo_group->currentGroupId();

    QList<ComputerConfig> computers = Database::instance().computerList(group_id);
    for (const ComputerConfig& existing : std::as_const(computers))
    {
        if (existing.id() != computer_id_ && existing.name() == name)
        {
            MsgBox::warning(this,
                tr("A computer with this name already exists in the selected group."));
            ui->edit_name->setFocus();
            return;
        }
    }

    ComputerConfig computer;
    computer.setId(computer_id_);
    computer.setGroupId(group_id);
    computer.setRouterId(router_id);
    computer.setName(ui->edit_name->text());
    computer.setAddress(ui->edit_address->text());
    computer.setUsername(ui->edit_username->text());
    computer.setPassword(ui->edit_password->password());
    computer.setComment(ui->edit_comment->toPlainText());

    Database& db = Database::instance();

    if (computer_id_ == -1)
    {
        if (!db.addComputer(computer))
        {
            MsgBox::warning(this, tr("Unable to add computer"));
            LOG(INFO) << "Unable to add computer to database";
            return;
        }
        computer_id_ = computer.id();
    }
    else
    {
        if (!db.modifyComputer(computer))
        {
            MsgBox::warning(this, tr("Unable to modify computer"));
            LOG(INFO) << "Unable to modify computer in database";
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void LocalComputerDialog::updateAddressLabel()
{
    if (ui->combo_router->currentData().toLongLong() == 0)
    {
        ui->label_address->setText(tr("Address:"));
        ui->edit_address->setPlaceholderText(tr("Computer name or IP address"));
    }
    else
    {
        ui->label_address->setText(tr("ID:"));
        ui->edit_address->setPlaceholderText(tr("Host ID"));
    }
}
