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

#include "client/desktop/management/router_group_dialog.h"

#include <QAbstractButton>

#include "base/logging.h"
#include "client/router_controller.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_group_dialog.h"

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

    // Counts characters, the bound counts UTF-8 bytes. A non-ASCII name is caught before sending.
    ui->edit_name->setMaxLength(static_cast<int>(proto::router::kMaxEntryNameLength));

    connect(ui->button_box, &QDialogButtonBox::clicked, this, &RouterGroupDialog::onButtonBoxClicked);

    // Disable input until the group list arrives. The combo and the existing group's data
    // (in modify mode) both depend on it.
    setEnabled(false);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
            reject();
    });

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        return;
    }

    session->listGroups(RouterSession::CachePolicy::USE_CACHE, workspace_id_,
                        { this, &RouterGroupDialog::onGroupListReceived });
}

//--------------------------------------------------------------------------------------------------
RouterGroupDialog::~RouterGroupDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDialog::onGroupListReceived(const RouterGroupList& list)
{
    if (list.error_code != proto::router::kErrorOk)
    {
        // Without the group tree the dialog is unusable: the parent combo stays empty and the
        // dialog disabled. Tell the operator and close instead of hanging.
        LOG(ERROR) << "Unable to get the list of the groups:" << list.error_code;
        MsgBox::warning(this, tr("Failed to get list of groups."));
        reject();
        return;
    }

    QList<GroupComboBox::Entry> entries;
    entries.reserve(list.groups.size());

    qint64 selected_parent = default_parent_id_;

    for (const RouterGroup& group : std::as_const(list.groups))
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
            base_revision_ = group.revision;
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

    LOG(ERROR) << "Group save failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, routerErrorText(error_code));
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

    RouterGroup group;
    group.entry_id  = entry_id_;
    group.parent_id = ui->combo_parent->currentGroupId();
    group.name      = name;
    group.comment   = ui->edit_comment->toPlainText();
    group.revision  = base_revision_;

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        MsgBox::warning(this, tr("Unknown internal error."));
        return;
    }

    // Disable the dialog while the request is in flight to prevent double-submits.
    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting group (entry_id:" << entry_id_
              << ", parent_id:" << group.parent_id << ")";
    if (entry_id_ > 0)
        session->modifyGroup(workspace_id_, group, { this, &RouterGroupDialog::onGroupResultReceived });
    else
        session->addGroup(workspace_id_, group, { this, &RouterGroupDialog::onGroupResultReceived });
}
