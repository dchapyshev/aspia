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

#include "client/android/router_editor.h"

#include <QVBoxLayout>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "proto/router.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterEditor::RouterEditor(QWidget* parent)
    : QWidget(parent),
      edit_name_(new LineEdit()),
      edit_address_(new LineEdit()),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      label_error_(new Label(QString(), Label::Role::CAPTION))
{
    edit_name_->setLabel(tr("Name"));
    edit_address_->setLabel(tr("Address"));
    edit_username_->setLabel(tr("User Name"));
    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

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
    form_layout->addWidget(edit_address_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(save);
    form_layout->addWidget(button_delete_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &RouterEditor::onSaveClicked);
    connect(button_delete_, &Button::clicked, this, &RouterEditor::onDeleteClicked);
}

//--------------------------------------------------------------------------------------------------
RouterEditor::~RouterEditor() = default;

//--------------------------------------------------------------------------------------------------
void RouterEditor::prepareForAdd()
{
    router_id_ = -1;

    edit_name_->clear();
    edit_address_->clear();
    edit_username_->clear();
    edit_password_->clear();
    label_error_->setVisible(false);
    button_delete_->hide();

    edit_name_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool RouterEditor::prepareForEdit(qint64 router_id)
{
    RouterConfig router;
    const Database::FindResult found = Database::instance().findRouter(router_id, &router);
    if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Router not found:" << router_id;
        return false;
    }

    router_id_ = router_id;

    edit_name_->setText(router.displayName());
    edit_address_->setText(router.address());
    edit_username_->setText(router.username());
    edit_password_->setText(router.password().toString());
    label_error_->setVisible(false);
    button_delete_->show();

    edit_name_->setFocus();

    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of router" << router_id << "could not be read";
        showError(tr("The data of the router could not be read. You can enter it again."));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onSaveClicked()
{
    if (edit_name_->text().length() > RouterConfig::kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", RouterConfig::kMaxNameLength));
        edit_name_->setFocus();
        edit_name_->selectAll();
        return;
    }

    const QString address_text = edit_address_->text();
    Address address = Address::fromString(address_text, kDefaultRouterClientTcpPort);
    if (!address.isValid())
    {
        showError(tr("An invalid router address was entered."));
        edit_address_->setFocus();
        edit_address_->selectAll();
        return;
    }

    const QString username = edit_username_->text();
    if (!User::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only alphabet characters,"
                     " numbers and \"_\", \"-\", \".\" characters."));
        edit_username_->setFocus();
        edit_username_->selectAll();
        return;
    }

    SecureString password(edit_password_->text());
    if (password.isEmpty())
    {
        showError(tr("Password cannot be empty."));
        edit_password_->setFocus();
        edit_password_->selectAll();
        return;
    }

    RouterConfig data;
    data.setRouterId(router_id_);
    data.setDisplayName(edit_name_->text());
    data.setAddress(address_text);
    data.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    data.setUsername(username);
    data.setPassword(password);

    Database& db = Database::instance();

    // The token is not edited here. The login of this record writes a fresh one whenever a TOTP
    // code is accepted, so it is read now instead of when the editor opened and a token issued
    // meanwhile survives the save. It belongs to the account of the record, and an edit that
    // changes the account leaves it behind, because presented for another one it would only be
    // refused.
    if (router_id_ >= 0)
    {
        RouterConfig stored;
        const Database::FindResult stored_found = db.findRouter(router_id_, &stored);
        if (stored_found == Database::FindResult::NOT_FOUND ||
            stored_found == Database::FindResult::FAILED)
        {
            LOG(ERROR) << "Failed to re-read router" << router_id_;
            showError(tr("Failed to save the router."));
            return;
        }
        if (stored.hasSameAccount(data))
            data.setDeviceToken(stored.deviceToken());
    }

    const bool saved = (router_id_ < 0) ? db.addRouter(data) : db.modifyRouter(data);
    if (!saved)
    {
        showError(tr("Failed to save the router."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onDeleteClicked()
{
    if (!MessageDialog::confirm(this, tr("Delete Router"),
                                tr("Delete the router \"%1\"?").arg(edit_name_->text()), tr("Delete")))
    {
        return;
    }

    if (!Database::instance().removeRouter(router_id_))
    {
        showError(tr("Failed to delete the router."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
