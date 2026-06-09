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

#include "client/ui/hosts/router_host_dialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QIcon>
#include <QPushButton>

#include "base/crypto/secure_string.h"
#include "base/logging.h"
#include "client/ui/hosts/group_combo_box.h"
#include "common/ui/msg_box.h"
#include "common/ui/password_edit.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"
#include "ui_router_host_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterHostDialog::RouterHostDialog(qint64 router_id, const QString& workspace_name,
                                   const Router::Host& host, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterHostDialog>()),
      router_id_(router_id),
      workspace_name_(workspace_name),
      host_(host)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_display_name->setText(host_.display_name);
    ui->edit_user_name->setText(host_.user_name);
    ui->edit_password->setPassword(host_.password);
    ui->edit_comment->setPlainText(host_.comment);

    ui->edit_password->setShowPasswordButtonVisible(true);
    connect(ui->button_box, &QDialogButtonBox::clicked, this, &RouterHostDialog::onButtonBoxClicked);

    // The group combo is populated asynchronously from listGroups(); disable Ok until the
    // response arrives so the user cannot submit before knowing which group they have selected.
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);

    Router* router = Router::instance(router_id_);
    CHECK(router);

    connect(router, &Router::sig_statusChanged, this, [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            reject();
    });

    router->listGroups(host_.workspace_id, this, &RouterHostDialog::onGroupListReceived);
}

//--------------------------------------------------------------------------------------------------
RouterHostDialog::~RouterHostDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterHostDialog::onGroupListReceived(const Router::GroupList& list)
{
    QList<GroupComboBox::Entry> entries;
    entries.reserve(list.groups.size());
    for (const Router::Group& group : std::as_const(list.groups))
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
        const char* message;

        if (error_code == proto::router::kErrorAccessDenied)
            message = QT_TR_NOOP("Access denied.");
        else if (error_code == proto::router::kErrorNotFound)
            message = QT_TR_NOOP("Host not found.");
        else if (error_code == proto::router::kErrorInvalidData)
            message = QT_TR_NOOP("Invalid data was passed.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
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

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router not available for id:" << router_id_;
        reject();
        return;
    }

    host_.display_name = ui->edit_display_name->text();
    host_.user_name    = ui->edit_user_name->text();
    host_.password     = ui->edit_password->password();
    host_.comment      = ui->edit_comment->toPlainText();
    host_.group_id     = ui->combo_group->currentGroupId();

    LOG(INFO) << "[ACTION] Edit host accepted, sending request";
    ui->button_box->button(QDialogButtonBox::Ok)->setEnabled(false);
    router->editHost(host_, this, &RouterHostDialog::onHostResultReceived);
}
