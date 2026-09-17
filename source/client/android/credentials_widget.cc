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

#include "client/android/credentials_widget.h"

#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/crypto/data_cryptor.h"
#include "client/config.h"
#include "client/database.h"
#include "client/android/credential_editor.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kCredentialIdRole = Qt::UserRole + 1;

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialsWidget::CredentialsWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      tree_(new TreeWidget()),
      editor_(new CredentialEditor(this)),
      add_button_(new IconButton(":/img/material/add_2.svg", this))
{
    // The add action lives in the app bar; AppBar::setActions() reparents and shows it. Hidden by
    // default so it does not linger in this widget.
    add_button_->hide();

    // Two columns: the name of the record and its user name.
    tree_->setRootIsDecorated(false);
    tree_->setColumnCount(2);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    QWidget* list_page = new QWidget(stack_);
    QVBoxLayout* list_layout = new QVBoxLayout(list_page);
    list_layout->setContentsMargins(0, 0, 0, 0);
    list_layout->setSpacing(0);
    list_layout->addWidget(tree_);

    stack_->addWidget(list_page);
    stack_->addWidget(editor_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(add_button_, &IconButton::clicked, this, &CredentialsWidget::onAddCredential);
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        onItemClicked(item);
    });
    connect(editor_, &CredentialEditor::sig_accepted, this, &CredentialsWidget::onReturnFromEditor);
}

//--------------------------------------------------------------------------------------------------
CredentialsWidget::~CredentialsWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> CredentialsWidget::appBarActions() const
{
    // The editor screen has its own form; no list actions there.
    if (isEditorPage())
        return {};
    return { add_button_ };
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::reload()
{
    tree_->clear();

    // The records are decrypted with the master-password-derived key, so there is nothing to show
    // until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
        return;

    const QIcon icon = GuiApplication::svgIcon(":/img/keys.svg");

    QList<CredentialConfig> credentials;
    Database::instance().credentialList(&credentials);
    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        QTreeWidgetItem* item =
            new QTreeWidgetItem(tree_, { credential.displayName(), credential.username() });
        item->setIcon(0, icon);
        item->setData(0, kCredentialIdRole, credential.id());
    }

    tree_->sortItems(0, Qt::AscendingOrder);
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::goBack()
{
    showList();
}

//--------------------------------------------------------------------------------------------------
bool CredentialsWidget::isEditorPage() const
{
    return stack_->currentWidget() == editor_;
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::onAddCredential()
{
    editor_->prepareForAdd();
    stack_->setCurrentWidget(editor_);
    emit sig_titleChanged(tr("Add Credentials"));
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::onItemClicked(QTreeWidgetItem* item)
{
    if (!item || !editor_->prepareForEdit(item->data(0, kCredentialIdRole).toLongLong()))
        return;

    stack_->setCurrentWidget(editor_);
    emit sig_titleChanged(tr("Edit Credentials"));
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::onReturnFromEditor()
{
    // The list changed (added, edited or removed), so it is rebuilt before returning.
    reload();
    showList();
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::showList()
{
    stack_->setCurrentIndex(0);
    emit sig_titleChanged(tr("Credentials"));
    emit sig_appBarActionsChanged();
}
