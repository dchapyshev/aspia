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

#include <QFileDialog>
#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/crypto/data_cryptor.h"
#include "client/config.h"
#include "client/database.h"
#include "client/android/credential_editor.h"
#include "client/android/credential_export_widget.h"
#include "client/android/credential_import_widget.h"
#include "common/android/icon_button.h"
#include "common/android/menu.h"
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
      export_(new CredentialExportWidget(this)),
      import_(new CredentialImportWidget(this)),
      button_add_(new IconButton(":/img/material/add_2.svg", this)),
      button_overflow_(new IconButton(":/img/material/more_vert.svg", this))
{
    // The actions live in the app bar; AppBar::setActions() reparents and shows them. Hidden by
    // default so they do not linger in this widget.
    button_add_->hide();
    button_overflow_->hide();

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
    stack_->addWidget(export_);
    stack_->addWidget(import_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(button_add_, &IconButton::clicked, this, &CredentialsWidget::onAddCredential);
    connect(button_overflow_, &IconButton::clicked, this, &CredentialsWidget::onShowMenu);
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        onItemClicked(item);
    });
    connect(editor_, &CredentialEditor::sig_accepted, this, &CredentialsWidget::onReturnFromEditor);
    connect(export_, &CredentialExportWidget::sig_finished, this, &CredentialsWidget::showList);
    connect(import_, &CredentialImportWidget::sig_finished, this, &CredentialsWidget::onReturnFromEditor);
}

//--------------------------------------------------------------------------------------------------
CredentialsWidget::~CredentialsWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> CredentialsWidget::appBarActions() const
{
    if (stack_->currentWidget() == export_)
        return export_->appBarActions();

    if (stack_->currentWidget() == import_)
        return import_->appBarActions();

    // The editor screen has its own form; no list actions there.
    if (!isListPage())
        return {};

    return { button_add_, button_overflow_ };
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
    const QIcon unread_icon = GuiApplication::svgIcon(":/img/key-corrupted.svg");

    QList<CredentialConfig> credentials;
    const Database::ReadResult result = Database::instance().credentialList(&credentials);
    if (result != Database::ReadResult::OK)
        LOG(ERROR) << "Unable to read the list of credentials:" << result;

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        QTreeWidgetItem* item =
            new QTreeWidgetItem(tree_, { credential.displayName(), credential.username() });
        item->setIcon(0, credential.isValid() ? icon : unread_icon);
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
bool CredentialsWidget::isListPage() const
{
    return stack_->currentIndex() == 0;
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
void CredentialsWidget::onShowMenu()
{
    enum { kImport, kExport };

    Menu* menu = new Menu(this);
    menu->addItem(tr("Import Credentials"), ":/img/material/download.svg");
    menu->addItem(tr("Export Credentials"), ":/img/material/upload.svg");

    connect(menu, &Menu::sig_triggered, this, [this](int index)
    {
        switch (index)
        {
            case kImport: onImport(); break;
            case kExport: onExport(); break;
            default: break;
        }
    });

    // Anchor the menu to the button; it drops from the button's near edge per layout direction.
    menu->popup(QRect(button_overflow_->mapToGlobal(QPoint(0, 0)), button_overflow_->size()));
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::onExport()
{
    export_->prepare();
    stack_->setCurrentWidget(export_);
    emit sig_titleChanged(tr("Export Credentials"));
    emit sig_appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void CredentialsWidget::onImport()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Credentials"), QString(), tr("Aspia Credentials (*.aspia-credentials)"));
    if (path.isEmpty())
        return;

    import_->prepare(path);
    stack_->setCurrentWidget(import_);
    emit sig_titleChanged(tr("Import Credentials"));
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
    export_->clear();
    import_->clear();

    stack_->setCurrentIndex(0);
    emit sig_titleChanged(tr("Credentials"));
    emit sig_appBarActionsChanged();
}
