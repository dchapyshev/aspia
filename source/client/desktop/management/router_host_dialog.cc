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

#include "client/desktop/management/router_host_dialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPushButton>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "client/desktop/management/group_combo_box.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "common/desktop/password_edit.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "ui_router_host_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterHostDialog::RouterHostDialog(qint64 router_id, const QString& workspace_name,
                                   const RouterHost& host, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterHostDialog>()),
      router_id_(router_id),
      workspace_name_(workspace_name),
      host_(host)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_display_name->setText(host_.display_name);
    ui->edit_comment->setPlainText(host_.comment);

    ui->edit_password->setShowPasswordButtonVisible(true);

    // A temporary host id is handed out at random and comes back for another machine, so what was
    // saved under it would be sent to a host the user never gave it to. Such a host is edited like
    // any other, it just has nowhere to keep credentials.
    if (isTempHostId(host_.host_id))
    {
        ui->edit_username->setEnabled(false);
        ui->edit_password->setEnabled(false);
    }
    else
    {
        std::optional<RouterHostConfig> credentials =
            Database::instance().findRouterHost(router_id_, host_.host_id);
        if (credentials.has_value())
        {
            ui->edit_username->setText(credentials->username());
            ui->edit_password->setPassword(credentials->password());
        }
    }

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &RouterHostDialog::onButtonBoxClicked);

    connect(&RouterController::instance(), &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
            reject();
    });

    // A host that belongs to no workspace lies in no group, and the router refuses a group list
    // request without a workspace. Such a host is edited with the combo left empty.
    if (host_.workspace_id <= 0)
    {
        ui->combo_group->setEnabled(false);
        return;
    }

    // The group combo is populated asynchronously from listGroups(); disable Ok until the
    // response arrives so the user cannot submit before knowing which group they have selected.
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        return;
    }

    session->listGroups(RouterSession::CachePolicy::USE_CACHE, host_.workspace_id,
                        { this, &RouterHostDialog::onGroupListReceived });
}

//--------------------------------------------------------------------------------------------------
RouterHostDialog::~RouterHostDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onGroupListReceived(const RouterGroupList& list)
{
    if (list.error_code != proto::router::kErrorOk)
    {
        // Without the group tree the dialog is unusable: the group combo stays empty and the
        // OK button disabled. Tell the operator and close instead of hanging.
        LOG(ERROR) << "Unable to get the list of the groups:" << list.error_code;
        MsgBox::warning(this, tr("Failed to get list of groups."));
        reject();
        return;
    }

    QList<GroupComboBox::Entry> entries;
    entries.reserve(list.groups.size());
    for (const RouterGroup& group : std::as_const(list.groups))
    {
        GroupComboBox::Entry& entry = entries.emplaceBack();
        entry.id = group.entry_id;
        entry.parent_id = group.parent_id;
        entry.name = group.name;
    }

    ui->combo_group->loadGroups(workspace_name_, QIcon(":/img/workspace.svg"), entries);
    ui->combo_group->selectGroup(host_.group_id);

    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onHostResultReceived(const proto::router::HostResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
        ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(true);
        return;
    }

    accept();
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->button_box->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Edit host rejected";
        reject();
        return;
    }

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        reject();
        return;
    }

    // The credentials are kept as a pair: an empty pair means the host is not remembered at all.
    if (ui->edit_username->text().isEmpty() != ui->edit_password->password().isEmpty())
    {
        MsgBox::warning(this, tr("Enter both the username and the password, or leave both empty."));
        return;
    }

    host_.display_name = ui->edit_display_name->text();
    host_.comment      = ui->edit_comment->toPlainText();
    host_.group_id     = ui->combo_group->currentGroupId();

    // The credentials live on this computer only, so they are not the router's to accept or
    // refuse. Kept for the answer, they would be lost with any error of it. A failure to keep them
    // stops here, with what was typed still in the form.
    if (!saveCredentials())
    {
        MsgBox::warning(this, tr("Failed to save the credentials."));
        return;
    }

    LOG(INFO) << "[ACTION] Edit host accepted, sending request";
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    session->editHost(host_, { this, &RouterHostDialog::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
bool RouterHostDialog::saveCredentials()
{
    if (isTempHostId(host_.host_id))
        return true;

    Database& db = Database::instance();

    const QString username = ui->edit_username->text();
    const SecureString password = ui->edit_password->password();

    if (username.isEmpty() && password.isEmpty())
    {
        if (db.removeRouterHost(router_id_, host_.host_id))
            return true;

        LOG(ERROR) << "Unable to remove credentials of host" << host_.host_id;
        return false;
    }

    RouterHostConfig credentials;
    credentials.setRouterId(router_id_);
    credentials.setHostId(host_.host_id);
    credentials.setUsername(username);
    credentials.setPassword(password);

    bool saved = false;

    if (db.findRouterHost(router_id_, host_.host_id).has_value())
        saved = db.modifyRouterHost(credentials);
    else
        saved = db.addRouterHost(credentials);

    if (!saved)
        LOG(ERROR) << "Unable to save credentials of host" << host_.host_id;

    return saved;
}
