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

#include "client/ui/hosts/router_group_dialog.h"

#include <QAbstractButton>
#include <QHash>

#include "base/logging.h"
#include "common/ui/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_group_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterGroupDialog::RouterGroupDialog(
    qint64 router_id, qint64 workspace_id, qint64 entry_id, qint64 default_parent_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterGroupDialog>()),
      router_id_(router_id),
      workspace_id_(workspace_id),
      entry_id_(entry_id),
      default_parent_id_(default_parent_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_name->setMaxLength(kMaxNameLength);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterGroupDialog::onButtonBoxClicked);

    // Disable input until the group list arrives. The combo and the existing group's data
    // (in modify mode) both depend on it.
    setEnabled(false);

    Router* router = Router::instance(router_id_);
    CHECK(router);
    router->listGroups(workspace_id_, this, &RouterGroupDialog::onGroupListReceived);
}

//--------------------------------------------------------------------------------------------------
RouterGroupDialog::~RouterGroupDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupDialog::onGroupListReceived(const Router::GroupList& list)
{
    // Index groups by id for O(1) parent-chain lookups below.
    QHash<qint64, const Router::Group*> by_id;
    for (const Router::Group& group : std::as_const(list.groups))
        by_id.insert(group.entry_id, &group);

    // In modify mode the group being edited and its descendants cannot be parents (would form
    // a cycle). Test membership by walking the parent chain up from each candidate; depth is
    // small in practice and the test runs once per combo item.
    auto in_own_subtree = [&](qint64 id)
    {
        if (entry_id_ <= 0)
            return false;
        while (id != 0)
        {
            if (id == entry_id_)
                return true;
            const Router::Group* group = by_id.value(id);
            if (!group)
                return false;
            id = group->parent_id;
        }
        return false;
    };

    ui->combo_parent->clear();
    ui->combo_parent->addItem(tr("(Workspace root)"), QVariant::fromValue<qint64>(0));

    for (const Router::Group& group : std::as_const(list.groups))
    {
        if (in_own_subtree(group.entry_id))
            continue;
        ui->combo_parent->addItem(group.name, QVariant::fromValue<qint64>(group.entry_id));
    }

    // Decide the initial parent selection. In modify mode also populate the form fields from
    // the existing group's row.
    qint64 selected_parent = default_parent_id_;
    if (entry_id_ > 0)
    {
        if (const Router::Group* self = by_id.value(entry_id_))
        {
            ui->edit_name->setText(self->name);
            ui->edit_comment->setPlainText(self->comment);
            selected_parent = self->parent_id;
        }
    }

    const int parent_index = ui->combo_parent->findData(QVariant::fromValue<qint64>(selected_parent));
    ui->combo_parent->setCurrentIndex(parent_index >= 0 ? parent_index : 0);

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
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
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
    group.parent_id = ui->combo_parent->currentData().value<qint64>();
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
