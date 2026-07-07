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

#include "client/desktop/hosts/router_group_dialog.h"

#include <QAbstractButton>

#include "base/logging.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_group_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterGroupDialog::RouterGroupDialog(
    qint64 router_id, qint64 workspace_id, const QString& workspace_name,
    qint64 entry_id, qint64 default_parent_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterGroupDialog>()),
      router_id_(router_id),
      workspace_id_(workspace_id),
      workspace_name_(workspace_name),
      entry_id_(entry_id),
      default_parent_id_(default_parent_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    setWindowTitle(entry_id_ > 0 ? tr("Edit Group") : tr("Add Group"));

    ui->edit_name->setMaxLength(kMaxNameLength);

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &RouterGroupDialog::onButtonBoxClicked);

    // Disable input until the group list arrives. The combo and the existing group's data
    // (in modify mode) both depend on it.
    setEnabled(false);

    Router* router = Router::instance(router_id_);
    CHECK(router);

    connect(router, &Router::sig_statusChanged, this, [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            reject();
    });

    router->listGroups(Router::CachePolicy::USE_CACHE, workspace_id_, this,
                       &RouterGroupDialog::onGroupListReceived);
}

//--------------------------------------------------------------------------------------------------
RouterGroupDialog::~RouterGroupDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDialog::onGroupListReceived(const Router::GroupList& list)
{
    QList<GroupComboBox::Entry> entries;
    entries.reserve(list.groups.size());

    qint64 selected_parent = default_parent_id_;

    for (const Router::Group& group : std::as_const(list.groups))
    {
        GroupComboBox::Entry& entry = entries.emplaceBack();
        entry.id = group.entry_id;
        entry.parent_id = group.parent_id;
        entry.name = group.name;

        if (entry_id_ > 0 && group.entry_id == entry_id_)
        {
            ui->edit_name->setText(group.name);
            ui->edit_comment->setPlainText(group.comment);
            selected_parent = group.parent_id;
        }
    }

    ui->combo_parent->loadGroups(workspace_name_, QIcon(":/img/workspace.svg"), entries, entry_id_);
    ui->combo_parent->selectGroup(selected_parent);

    setEnabled(true);
    ui->edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDialog::onGroupResultReceived(const proto::router::GroupResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] Group saved";
        accept();
        close();
        return;
    }

    const char* message;
    if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TR_NOOP("Invalid group request.");
    else if (error_code == proto::router::kErrorInternalError)
        message = QT_TR_NOOP("Unknown internal error.");
    else if (error_code == proto::router::kErrorInvalidData)
        message = QT_TR_NOOP("Invalid data was passed.");
    else if (error_code == proto::router::kErrorAccessDenied)
        message = QT_TR_NOOP("Access denied.");
    else if (error_code == proto::router::kErrorNotFound)
        message = QT_TR_NOOP("Group not found.");
    else
        message = QT_TR_NOOP("Unknown error type.");

    LOG(ERROR) << "Group save failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, tr(message));
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->button_box->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
    }

    const QString name = ui->edit_name->text().trimmed();
    if (name.isEmpty())
    {
        LOG(ERROR) << "Empty group name";
        MsgBox::warning(this, tr("Group name cannot be empty."));
        ui->edit_name->setFocus();
        return;
    }

    Router::Group group;
    group.entry_id  = entry_id_;
    group.parent_id = ui->combo_parent->currentGroupId();
    group.name      = name;
    group.comment   = ui->edit_comment->toPlainText();

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance not found:" << router_id_;
        MsgBox::warning(this, tr("Unknown internal error."));
        return;
    }

    // Disable the dialog while the request is in flight to prevent double-submits.
    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting group (entry_id:" << entry_id_
              << ", parent_id:" << group.parent_id << ")";
    if (entry_id_ > 0)
        router->modifyGroup(workspace_id_, group, this, &RouterGroupDialog::onGroupResultReceived);
    else
        router->addGroup(workspace_id_, group, this, &RouterGroupDialog::onGroupResultReceived);
}
