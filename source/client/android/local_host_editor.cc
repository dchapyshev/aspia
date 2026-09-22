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

#include "client/android/local_host_editor.h"

#include <QVBoxLayout>

#include "base/build_config.h"
#include "base/gui_application.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "common/android/text_area.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostEditor::LocalHostEditor(QWidget* parent)
    : QWidget(parent),
      combo_router_(new ComboBox()),
      edit_name_(new LineEdit()),
      edit_address_(new LineEdit()),
      switch_saved_credentials_(new Switch(tr("Use existing"))),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      combo_credential_(new ComboBox()),
      edit_comment_(new TextArea()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_name_->setLabel(tr("Name"));
    combo_router_->setLabel(tr("Router"));
    edit_address_->setLabel(tr("Address"));
    edit_username_->setLabel(tr("User Name"));
    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);
    combo_credential_->setLabel(tr("Credentials"));
    edit_comment_->setLabel(tr("Comment"));

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    // The delete action is destructive, so its text is tinted red and it shows only when editing.
    button_delete_ = new Button(tr("Delete"), Button::Role::TEXT);
    button_delete_->setAccentColor(Controls::errorColor());
    button_delete_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_name_);
    form_layout->addWidget(combo_router_);
    form_layout->addWidget(edit_address_);
    form_layout->addWidget(switch_saved_credentials_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(combo_credential_);
    form_layout->addWidget(edit_comment_);
    form_layout->addWidget(save);
    form_layout->addWidget(button_delete_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(combo_router_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalHostEditor::onRouterChanged);
    connect(switch_saved_credentials_, &Switch::toggled, this, &LocalHostEditor::onSavedCredentialsToggled);
    connect(save, &Button::clicked, this, &LocalHostEditor::onSaveClicked);
    connect(button_delete_, &Button::clicked, this, &LocalHostEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
LocalHostEditor::~LocalHostEditor() = default;

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::prepareForAdd(qint64 group_id)
{
    entry_id_ = -1;
    group_id_ = group_id;

    edit_name_->clear();
    edit_address_->clear();
    edit_username_->clear();
    edit_password_->clear();
    edit_comment_->clear();
    label_error_->setVisible(false);
    button_delete_->hide();

    const bool routers_loaded = loadRouters(0);
    onRouterChanged();
    const bool credentials_loaded = loadCredentials(0);
    lists_loaded_ = routers_loaded && credentials_loaded;

    edit_name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool LocalHostEditor::prepareForEdit(qint64 host_id)
{
    LocalHostConfig host;
    const Database::FindResult found = Database::instance().findLocalHost(host_id, &host);
    if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Host not found:" << host_id;
        return false;
    }

    entry_id_ = host_id;
    group_id_ = host.groupId();

    edit_name_->setText(host.name());
    edit_address_->setText(host.address());
    edit_username_->setText(host.username());
    edit_password_->setText(host.password().toString());
    edit_comment_->setText(host.comment());
    label_error_->setVisible(false);
    button_delete_->show();

    const bool routers_loaded = loadRouters(host.routerId());
    onRouterChanged();
    const bool credentials_loaded = loadCredentials(host.credentialId());
    lists_loaded_ = routers_loaded && credentials_loaded;

    edit_name_->setFocus();

    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of host" << host_id << "could not be read";
        if (lists_loaded_)
            showError(tr("The data of the host could not be read. You can enter it again."));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostEditor::loadRouters(qint64 selected_router_id)
{
    combo_router_->clear();
    combo_router_->addItem(tr("Without Router"), QVariant::fromValue<qint64>(0));

    QList<RouterConfig> routers;
    const Database::ReadResult result = Database::instance().routerList(&routers);
    if (result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of routers";
        showError(tr("Failed to read data from the local database."));
        return false;
    }

    if (result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the routers";

    for (const RouterConfig& router : std::as_const(routers))
        combo_router_->addItem(router.displayLabel(), QVariant::fromValue(router.routerId()));

    const int index = combo_router_->findData(QVariant::fromValue(selected_router_id));
    combo_router_->setCurrentIndex(index >= 0 ? index : 0);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostEditor::loadCredentials(qint64 selected_credential_id)
{
    combo_credential_->clear();

    QList<CredentialConfig> credentials;
    const Database::ReadResult result = Database::instance().credentialList(&credentials);
    if (result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of credentials";
        showError(tr("Failed to read data from the local database."));
        return false;
    }

    if (result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the credentials";

    const QIcon icon = GuiApplication::svgIcon(":/img/keys.svg");
    const QIcon unread_icon = GuiApplication::svgIcon(":/img/key-corrupted.svg");

    for (const CredentialConfig& credential : std::as_const(credentials))
    {
        combo_credential_->addItem(credential.isValid() ? icon : unread_icon,
                                   credential.displayName(),
                                   QVariant::fromValue(credential.id()));
    }

    const int index = combo_credential_->findData(QVariant::fromValue(selected_credential_id));
    combo_credential_->setCurrentIndex(index >= 0 ? index : 0);

    // Nothing to share until a record of credentials is added.
    switch_saved_credentials_->setEnabled(!credentials.isEmpty());
    switch_saved_credentials_->setChecked(index >= 0);
    onSavedCredentialsToggled(switch_saved_credentials_->isChecked());
    return true;
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onRouterChanged()
{
    // Without a router the address is a host name or IP; through a router it is a host ID.
    const bool direct = (combo_router_->currentData().toLongLong() == 0);
    edit_address_->setLabel(direct ? tr("Address") : tr("ID"));
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onSavedCredentialsToggled(bool checked)
{
    if (checked && edit_username_->text().isEmpty() != edit_password_->text().isEmpty())
    {
        edit_username_->clear();
        edit_password_->clear();
    }

    // Entered with a record of credentials, the host shows the record in place of its own pair.
    edit_username_->setVisible(!checked);
    edit_password_->setVisible(!checked);
    combo_credential_->setVisible(checked);
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onSaveClicked()
{
    if (!lists_loaded_)
    {
        showError(tr("Failed to read data from the local database."));
        return;
    }

    const QString name = edit_name_->text();
    if (name.isEmpty())
    {
        showError(tr("Name cannot be empty."));
        edit_name_->setFocus();
        return;
    }

    if (name.length() > LocalHostConfig::kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", LocalHostConfig::kMaxNameLength));
        edit_name_->setFocus();
        edit_name_->selectAll();
        return;
    }

    if (edit_comment_->text().length() > LocalHostConfig::kMaxCommentLength)
    {
        showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                     "", LocalHostConfig::kMaxCommentLength));
        edit_comment_->setFocus();
        return;
    }

    const qint64 router_id = combo_router_->currentData().toLongLong();
    const QString address_text = edit_address_->text();

    if (router_id == 0)
    {
        if (!Address::fromString(address_text, kDefaultHostTcpPort).isValid())
        {
            showError(tr("An invalid host address was entered."));
            edit_address_->setFocus();
            edit_address_->selectAll();
            return;
        }
    }
    else if (!isHostId(address_text))
    {
        showError(tr("An invalid host ID was entered."));
        edit_address_->setFocus();
        edit_address_->selectAll();
        return;
    }

    const QString username = edit_username_->text();
    const QString password = edit_password_->text();

    if (!username.isEmpty() && !User::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only alphabet characters,"
                     " numbers and \"_\", \"-\", \".\" characters."));
        edit_username_->setFocus();
        edit_username_->selectAll();
        return;
    }

    if (username.isEmpty() != password.isEmpty())
    {
        showError(tr("Enter both the user name and the password, or leave both empty."));
        return;
    }

    LocalHostConfig data;
    data.setId(entry_id_);
    data.setGroupId(group_id_);
    data.setRouterId(router_id);
    data.setName(name);
    data.setAddress(address_text);
    data.setCredentialId(switch_saved_credentials_->isChecked() ? combo_credential_->currentData().toLongLong() : 0);
    data.setUsername(username);
    data.setPassword(SecureString(password));
    data.setComment(edit_comment_->text());

    Database& db = Database::instance();
    const bool saved = (entry_id_ < 0) ? db.addLocalHost(data) : db.modifyLocalHost(data);
    if (!saved)
    {
        showError(tr("Failed to save the host."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Host"),
                                tr("Delete the host \"%1\"?").arg(edit_name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeLocalHost(entry_id_))
    {
        showError(tr("Failed to delete the host."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void LocalHostEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
