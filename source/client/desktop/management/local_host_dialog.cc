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

#include "client/desktop/management/local_host_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <algorithm>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/database.h"
#include "client/settings.h"
#include "client/desktop/management/group_combo_box.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "ui_local_host_dialog.h"

namespace {

constexpr int kMinNameLength = 1;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostDialog::LocalHostDialog(qint64 entry_id, qint64 group_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::LocalHostDialog>()),
      entry_id_(entry_id),
      group_id_(group_id)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    Settings settings;
    restoreGeometry(settings.dialogGeometry(objectName()));

    setWindowTitle(entry_id_ != -1 ? tr("Edit Host") : tr("Add Host"));

    ui->edit_password->setShowPasswordButtonVisible(true);
    connect(ui->combo_router, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalHostDialog::onRouterChanged);
    connect(ui->checkbox_saved_credentials, &QCheckBox::toggled, this, &LocalHostDialog::onSavedCredentialsToggled);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &LocalHostDialog::onButtonBoxClicked);

    int label_width = 0;
    for (const QLabel* label : { ui->label_group, ui->label_name, ui->label_router,
                                 ui->label_address, ui->label_username, ui->label_password,
                                 ui->label_credential })
    {
        label_width = std::max(label_width, label->sizeHint().width());
    }

    ui->gridLayout->setColumnMinimumWidth(0, label_width);

    onSavedCredentialsToggled(ui->checkbox_saved_credentials->isChecked());

    ui->edit_name->setFocus();

    QTimer::singleShot(MilliSeconds::zero(), this, &LocalHostDialog::onLoadData);
}

//--------------------------------------------------------------------------------------------------
LocalHostDialog::~LocalHostDialog()
{
    LOG(INFO) << "Dtor";

    Settings settings;
    settings.setDialogGeometry(objectName(), saveGeometry());
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::onRouterChanged(int /* index */)
{
    updateAddressLabel();
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::onSavedCredentialsToggled(bool checked)
{
    if (checked && ui->edit_username->text().isEmpty() != ui->edit_password->password().isEmpty())
    {
        ui->edit_username->clear();
        ui->edit_password->clear();
    }

    ui->label_username->setVisible(!checked);
    ui->edit_username->setVisible(!checked);
    ui->label_password->setVisible(!checked);
    ui->edit_password->setVisible(!checked);
    ui->label_credential->setVisible(checked);
    ui->combo_credential->setVisible(checked);
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

    if (name.length() > LocalHostConfig::kMaxNameLength)
    {
        MsgBox::warning(this,
            tr("Too long name. The maximum length of the name is %n characters.",
               "", LocalHostConfig::kMaxNameLength));
        ui->edit_name->setFocus();
        ui->edit_name->selectAll();
        return;
    }

    qint64 router_id = ui->combo_router->currentData().toLongLong();

    if (router_id == 0)
    {
        Address address =
            Address::fromString(ui->edit_address->text(), kDefaultHostTcpPort);
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

    const QString username = ui->edit_username->text();
    const SecureString password = ui->edit_password->password();

    if (!username.isEmpty() && !User::isValidUserName(username))
    {
        MsgBox::warning(this,
            tr("The user name can not be empty and can contain only"
               " alphabet characters, numbers and \"_\", \"-\", \".\" characters."));
        ui->edit_username->setFocus();
        ui->edit_username->selectAll();
        return;
    }

    if (username.isEmpty() != password.isEmpty())
    {
        MsgBox::warning(this, tr("Enter both the user name and the password, or leave both empty."));
        return;
    }

    QString comment = ui->edit_comment->toPlainText();
    if (comment.length() > LocalHostConfig::kMaxCommentLength)
    {
        MsgBox::warning(this,
            tr("Too long comment. The maximum length of the comment is %n characters.",
               "", LocalHostConfig::kMaxCommentLength));
        ui->edit_comment->setFocus();
        ui->edit_comment->selectAll();
        return;
    }

    qint64 group_id = ui->combo_group->currentGroupId();

    QList<LocalHostConfig> hosts;
    if (Database::instance().localHostList(group_id, &hosts) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the hosts of the selected group";
        MsgBox::warning(this, tr("Failed to read data from the local database."));
        return;
    }

    for (const LocalHostConfig& existing : std::as_const(hosts))
    {
        if (existing.id() != entry_id_ && existing.name() == name)
        {
            MsgBox::warning(this,
                tr("A host with this name already exists in the selected group."));
            ui->edit_name->setFocus();
            return;
        }
    }

    LocalHostConfig host;
    host.setId(entry_id_);
    host.setGroupId(group_id);
    host.setRouterId(router_id);
    host.setName(ui->edit_name->text());
    host.setAddress(ui->edit_address->text());
    host.setCredentialId(
        ui->checkbox_saved_credentials->isChecked() ? ui->combo_credential->currentData().toLongLong() : 0);
    host.setUsername(username);
    host.setPassword(password);
    host.setComment(ui->edit_comment->toPlainText());

    Database& db = Database::instance();

    if (entry_id_ == -1)
    {
        if (!db.addLocalHost(host))
        {
            MsgBox::warning(this, tr("Unable to add host"));
            LOG(INFO) << "Unable to add host to database";
            return;
        }
        entry_id_ = host.id();
    }
    else
    {
        if (!db.modifyLocalHost(host))
        {
            MsgBox::warning(this, tr("Unable to modify host"));
            LOG(INFO) << "Unable to modify host in database";
            return;
        }
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void LocalHostDialog::onLoadData()
{
    Database& db = Database::instance();

    ui->combo_router->addItem(QIcon(":/img/connect.svg"), tr("Without Router"),
                              QVariant::fromValue<qint64>(0));

    QList<RouterConfig> routers;
    const Database::ReadResult routers_result = db.routerList(&routers);
    if (routers_result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of routers";
        MsgBox::warning(this, tr("Failed to read the list of routers."));
        reject();
        return;
    }

    if (routers_result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the routers";

    const QIcon router_icon(":/img/stack.svg");

    for (const RouterConfig& router : std::as_const(routers))
    {
        ui->combo_router->addItem(router_icon, router.displayLabel(),
                                  QVariant::fromValue(router.routerId()));
    }

    QList<CredentialConfig> credentials;
    const Database::ReadResult credentials_result = db.credentialList(&credentials);
    if (credentials_result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of credentials";
        MsgBox::warning(this, tr("Failed to read the list of credentials."));
        reject();
        return;
    }

    if (credentials_result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the credentials";

    const QIcon credential_icon(":/img/keys.svg");
    const QIcon unread_credential_icon(":/img/key-corrupted.svg");

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        ui->combo_credential->addItem(credential.isValid() ? credential_icon : unread_credential_icon,
                                      credential.displayName(),
                                      QVariant::fromValue(credential.id()));
    }

    QList<LocalGroupConfig> all_groups;
    if (!db.allLocalGroups(&all_groups))
    {
        LOG(ERROR) << "Unable to read the list of groups";
        MsgBox::warning(this, tr("Failed to read the list of groups."));
        reject();
        return;
    }

    QList<GroupComboBox::Entry> group_entries;
    group_entries.reserve(all_groups.size());

    for (const LocalGroupConfig& group : std::as_const(all_groups))
    {
        GroupComboBox::Entry& entry = group_entries.emplaceBack();
        entry.id = group.id();
        entry.parent_id = group.parentId();
        entry.name = group.name();
    }

    ui->combo_group->loadGroups(tr("Local"), QIcon(":/img/folder.svg"), group_entries);

    qint64 selected_router_id = 0;
    Database::FindResult host_found = Database::FindResult::NOT_FOUND;

    if (entry_id_ != -1)
    {
        LocalHostConfig host;
        host_found = db.findLocalHost(entry_id_, &host);

        if (host_found == Database::FindResult::NOT_FOUND || host_found == Database::FindResult::FAILED)
        {
            LOG(ERROR) << "Unable to find host with id" << entry_id_;
            MsgBox::warning(this, tr("Failed to retrieve host information from the local database."));
            reject();
            return;
        }

        ui->edit_name->setText(host.name());
        ui->edit_address->setText(host.address());
        ui->edit_username->setText(host.username());
        ui->edit_password->setPassword(host.password());
        ui->edit_comment->setPlainText(host.comment());

        if (host.credentialId() > 0)
        {
            ui->checkbox_saved_credentials->setChecked(true);
            ui->combo_credential->setCurrentIndex(
                ui->combo_credential->findData(QVariant::fromValue(host.credentialId())));
        }

        group_id_ = host.groupId();
        selected_router_id = host.routerId();
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

    ui->combo_group->selectGroup(group_id_);

    // Nothing to share until a record of credentials is added. A record that did not open is
    // in the list too, so a host never refers to one the combo does not hold.
    ui->checkbox_saved_credentials->setEnabled(ui->combo_credential->count() > 0);

    updateAddressLabel();

    onSavedCredentialsToggled(ui->checkbox_saved_credentials->isChecked());

    if (host_found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of host" << entry_id_ << "could not be read";
        MsgBox::warning(this, tr("The data of the host could not be read. You can enter it again."));
    }
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
