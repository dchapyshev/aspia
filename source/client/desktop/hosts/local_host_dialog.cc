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

#include "client/desktop/hosts/local_host_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QPushButton>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"
#include "client/config.h"
#include "client/database.h"
#include "client/desktop/hosts/group_combo_box.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "ui_local_host_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostDialog::LocalHostDialog(qint64 entry_id, qint64 group_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::LocalHostDialog>()),
      entry_id_(entry_id)
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

    if (entry_id_ != -1)
    {
        setWindowTitle(tr("Edit Host"));

        std::optional<HostConfig> host = Database::instance().findHost(entry_id_);
        if (host.has_value())
        {
            ui->edit_name->setText(host->name());
            ui->edit_address->setText(host->address());
            ui->edit_username->setText(host->username());
            ui->edit_password->setPassword(host->password());
            ui->edit_comment->setPlainText(host->comment());
            group_id_ = host->groupId();
            selected_router_id = host->routerId();
        }
        else
        {
            LOG(ERROR) << "Unable to find host with id" << entry_id_;
        }
    }
    else
    {
        setWindowTitle(tr("Add Host"));
        group_id_ = group_id;
    }

    if (selected_router_id != 0)
    {
        int found_index = ui->combo_router->findData(QVariant::fromValue(selected_router_id));
        if (found_index < 0)
        {
            LOG(WARNING) << "Host references missing router id" << selected_router_id;
            ui->combo_router->addItem(QIcon(":/img/high-importance.svg"), tr("<deleted router>"),
                                     QVariant::fromValue(selected_router_id));
            found_index = ui->combo_router->count() - 1;
        }
        ui->combo_router->setCurrentIndex(found_index);
    }

    updateAddressLabel();

    const QList<GroupConfig> all_groups = Database::instance().allGroups();

    QList<GroupComboBox::Entry> group_entries;
    group_entries.reserve(all_groups.size());

    for (const GroupConfig& group : std::as_const(all_groups))
    {
        GroupComboBox::Entry& entry = group_entries.emplaceBack();
        entry.id = group.id();
        entry.parent_id = group.parentId();
        entry.name = group.name();
    }

    ui->combo_group->loadGroups(tr("Local"), QIcon(":/img/folder.svg"), group_entries);
    ui->combo_group->selectGroup(group_id_);

    ui->edit_password->setShowPasswordButtonVisible(true);
    connect(ui->combo_router, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalHostDialog::onRouterChanged);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &LocalHostDialog::onButtonBoxClicked);

    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
LocalHostDialog::~LocalHostDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::onRouterChanged(int /* index */)
{
    updateAddressLabel();
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::onButtonBoxClicked(QAbstractButton* button)
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
            MsgBox::warning(this, tr("An invalid host address was entered."));
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

    QList<HostConfig> hosts = Database::instance().hostList(group_id);
    for (const HostConfig& existing : std::as_const(hosts))
    {
        if (existing.id() != entry_id_ && existing.name() == name)
        {
            MsgBox::warning(this,
                tr("A host with this name already exists in the selected group."));
            ui->edit_name->setFocus();
            return;
        }
    }

    HostConfig host;
    host.setId(entry_id_);
    host.setGroupId(group_id);
    host.setRouterId(router_id);
    host.setName(ui->edit_name->text());
    host.setAddress(ui->edit_address->text());
    host.setUsername(ui->edit_username->text());
    host.setPassword(ui->edit_password->password());
    host.setComment(ui->edit_comment->toPlainText());

    Database& db = Database::instance();

    if (entry_id_ == -1)
    {
        if (!db.addHost(host))
        {
            MsgBox::warning(this, tr("Unable to add host"));
            LOG(INFO) << "Unable to add host to database";
            return;
        }
        entry_id_ = host.id();
    }
    else
    {
        if (!db.modifyHost(host))
        {
            MsgBox::warning(this, tr("Unable to modify host"));
            LOG(INFO) << "Unable to modify host in database";
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::updateAddressLabel()
{
    if (ui->combo_router->currentData().toLongLong() == 0)
    {
        ui->label_address->setText(tr("Address:"));
        ui->edit_address->setPlaceholderText(tr("Host name or IP address"));
    }
    else
    {
        ui->label_address->setText(tr("ID:"));
        ui->edit_address->setPlaceholderText(tr("Host ID"));
    }
}
