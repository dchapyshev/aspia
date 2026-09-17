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

#include "client/desktop/credentials_tab.h"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>

#include "base/logging.h"
#include "client/database.h"
#include "client/desktop/credential_dialog.h"
#include "client/desktop/credential_list_model.h"
#include "common/desktop/msg_box.h"
#include "ui_credentials_tab.h"

//--------------------------------------------------------------------------------------------------
CredentialsTab::CredentialsTab(QWidget* parent)
    : Tab(Type::CREDENTIALS, "credentials", parent),
      ui(std::make_unique<Ui::CredentialsTab>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    model_ = new CredentialListModel(this);
    ui->tree_credentials->setModel(model_);

    ui->tree_credentials->setSortingEnabled(true);
    ui->tree_credentials->sortByColumn(
        static_cast<int>(CredentialListModel::Column::NAME), Qt::AscendingOrder);

    connect(ui->tree_credentials->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CredentialsTab::onSelectionChanged);
    connect(ui->tree_credentials, &QAbstractItemView::activated,
            this, [this](const QModelIndex&) { onEditAction(); });

    ui->tree_credentials->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_credentials, &QWidget::customContextMenuRequested,
            this, &CredentialsTab::onContextMenu);

    connect(ui->action_add, &QAction::triggered, this, &CredentialsTab::onAddAction);
    connect(ui->action_edit, &QAction::triggered, this, &CredentialsTab::onEditAction);
    connect(ui->action_delete, &QAction::triggered, this, &CredentialsTab::onDeleteAction);

    addActions(ActionRole::EDIT, { ui->action_add, ui->action_edit, ui->action_delete });

    reload(-1);
}

//--------------------------------------------------------------------------------------------------
CredentialsTab::~CredentialsTab()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QByteArray CredentialsTab::saveState()
{
    return ui->tree_credentials->header()->saveState();
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::restoreState(const QByteArray& state)
{
    if (state.isEmpty())
        return;

    ui->tree_credentials->header()->restoreState(state);
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::activate(QStatusBar* /* statusbar */)
{
    const CredentialConfig* credential = selectedCredential();
    reload(credential ? credential->id() : -1);
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::deactivate(QStatusBar* /* statusbar */)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
bool CredentialsTab::hasStatusBar() const
{
    return false;
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::onAddAction()
{
    LOG(INFO) << "[ACTION] Add credentials";

    CredentialDialog dialog(-1, this);
    if (dialog.exec() == CredentialDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reload(dialog.credentialId());
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::onEditAction()
{
    LOG(INFO) << "[ACTION] Edit credentials";

    const CredentialConfig* credential = selectedCredential();
    if (!credential)
        return;

    const qint64 credential_id = credential->id();

    CredentialDialog dialog(credential_id, this);
    if (dialog.exec() == CredentialDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reload(credential_id);
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::onDeleteAction()
{
    LOG(INFO) << "[ACTION] Delete credentials";

    const CredentialConfig* credential = selectedCredential();
    if (!credential)
        return;

    if (MsgBox::question(this,
            tr("Are you sure you want to delete credentials \"%1\"?").arg(credential->displayName()),
            MsgBox::Yes | MsgBox::No) != MsgBox::Yes)
    {
        return;
    }

    if (!Database::instance().removeCredential(credential->id()))
    {
        MsgBox::warning(this, tr("Unable to delete credentials."));
        return;
    }

    reload(-1);
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::onSelectionChanged()
{
    const bool selected = selectedCredential() != nullptr;
    ui->action_edit->setEnabled(selected);
    ui->action_delete->setEnabled(selected);
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::onContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_credentials->indexAt(pos);

    QMenu menu;

    if (index.isValid())
    {
        ui->tree_credentials->setCurrentIndex(index);
        menu.addAction(ui->action_edit);
        menu.addAction(ui->action_delete);
    }
    else
    {
        menu.addAction(ui->action_add);
    }

    menu.exec(ui->tree_credentials->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void CredentialsTab::reload(qint64 credential_id)
{
    QList<CredentialConfig> credentials;
    if (!Database::instance().credentialList(&credentials))
        LOG(ERROR) << "Unable to read credentials";

    model_->setCredentials(credentials);

    const int row = model_->rowOf(credential_id);
    if (row >= 0)
    {
        ui->tree_credentials->selectionModel()->select(
            model_->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }

    onSelectionChanged();
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* CredentialsTab::selectedCredential() const
{
    const QModelIndexList rows = ui->tree_credentials->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return nullptr;

    return model_->credentialAt(rows.front().row());
}
