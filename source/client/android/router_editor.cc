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
#include "client/router/router_controller.h"
#include "client/storage/database.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/message_dialog.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"
#include "proto/router.h"

namespace {

constexpr int kFormMargin = 16;
constexpr int kFormSpacing = 8;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterEditor::RouterEditor(QWidget* parent)
    : QWidget(parent),
      edit_address_(new LineEdit()),
      edit_username_(new LineEdit()),
      edit_password_(new LineEdit()),
      switch_enabled_(new Switch(tr("Enabled"))),
      switch_sync_(new Switch(tr("Synchronize data"))),
      label_error_(new Label(QString(), Label::Role::CAPTION)),
      button_reset_sync_(new Button(tr("Reset Synchronization"), Button::Role::TEXT))
{
    edit_address_->setLabel(tr("Address"));
    edit_username_->setLabel(tr("User Name"));
    edit_username_->setMaxLength(static_cast<int>(User::kMaxUserNameLength));
    edit_password_->setLabel(tr("Password"));
    edit_password_->setEchoMode(QLineEdit::Password);

    // A fixed hex keeps the error color readable on both light and dark surfaces and survives the
    // palette reset that the caption role applies on theme changes.
    label_error_->setStyleSheet(QString("color: %1;").arg(Controls::errorColor().name()));
    label_error_->setWordWrap(true);
    label_error_->setVisible(false);

    Button* save = new Button(tr("Save"), Button::Role::FILLED);

    button_reset_sync_->setAccentColor(Controls::errorColor());
    button_reset_sync_->hide();

    QWidget* form = new QWidget();
    QVBoxLayout* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(kFormMargin, kFormMargin, kFormMargin, kFormMargin);
    form_layout->setSpacing(kFormSpacing);
    form_layout->addWidget(label_error_);
    form_layout->addWidget(edit_address_);
    form_layout->addWidget(edit_username_);
    form_layout->addWidget(edit_password_);
    form_layout->addWidget(switch_enabled_);
    form_layout->addWidget(switch_sync_);
    form_layout->addWidget(save);
    form_layout->addWidget(button_reset_sync_);
    form_layout->addStretch();

    ScrollArea* scroll = new ScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(form);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    connect(save, &Button::clicked, this, &RouterEditor::onSaveClicked);
    connect(button_reset_sync_, &Button::clicked, this, &RouterEditor::onResetSyncClicked);
    connect(&RouterController::instance(), &RouterController::sig_statusChanged,
            this, &RouterEditor::onRouterStatusChanged);
}

//--------------------------------------------------------------------------------------------------
RouterEditor::~RouterEditor() = default;

//--------------------------------------------------------------------------------------------------
void RouterEditor::prepareForAdd()
{
    edit_address_->clear();
    edit_username_->clear();
    edit_password_->clear();
    switch_enabled_->setChecked(true);
    switch_sync_->setChecked(false);
    label_error_->setVisible(false);
    button_reset_sync_->hide();

    edit_address_->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool RouterEditor::prepareForEdit()
{
    RouterConfig router;
    const Database::FindResult found = Database::instance().routerConfig(&router);
    if (found == Database::FindResult::NOT_FOUND || found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Router not found:" << found;
        return false;
    }

    edit_address_->setText(router.address());
    edit_username_->setText(router.username());
    edit_password_->setText(router.password().toString());
    switch_enabled_->setChecked(router.isEnabled());
    switch_sync_->setChecked(router.isSyncEnabled());
    label_error_->setVisible(false);

    onRouterStatusChanged();

    edit_address_->setFocus();

    if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Data of the router could not be read";
        showError(tr("The data of the router is damaged. Enter it again."));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onSaveClicked()
{
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
    data.setAddress(address_text);
    data.setSessionType(proto::router::SESSION_TYPE_OPERATOR);
    data.setUsername(username);
    data.setPassword(password);
    data.setEnabled(switch_enabled_->isChecked());
    data.setSyncEnabled(switch_sync_->isChecked());

    Database& db = Database::instance();

    RouterConfig stored;
    const Database::FindResult stored_found = db.routerConfig(&stored);
    if (stored_found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Failed to re-read the router";
        showError(tr("Failed to save the router."));
        return;
    }

    if (stored_found == Database::FindResult::FOUND && stored.hasSameAccount(data))
    {
        if (stored.password() == data.password())
            data.setDeviceToken(stored.deviceToken());
    }

    if (stored.isSyncConfirmationNeeded(data))
    {
        const QString text =
            tr("The synchronized data of this device will be sent to the storage of the account \"%1\". "
               "Continue?").arg(username);

        if (!MessageDialog::confirm(this, tr("Confirmation"), text, tr("Continue")))
            return;
    }

    if (!db.setRouterConfig(data))
    {
        showError(tr("Failed to save the router."));
        return;
    }

    emit sig_accepted();
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onResetSyncClicked()
{
    const QString text =
        tr("The synchronized data on the router and on the other devices of the account will be replaced with "
           "the data of this device. Reset the synchronization?");

    if (!MessageDialog::confirm(this, tr("Reset Synchronization"), text, tr("Reset")))
        return;

    if (!RouterController::resetSyncStorage())
        showError(tr("Failed to reset the synchronization."));
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::onRouterStatusChanged()
{
    RouterConfig router;
    const bool sync_enabled =
        Database::instance().routerConfig(&router) == Database::FindResult::FOUND && router.isSyncEnabled();

    button_reset_sync_->setVisible(sync_enabled && RouterController::status() == RouterStatus::ONLINE);
}

//--------------------------------------------------------------------------------------------------
void RouterEditor::showError(const QString& message)
{
    label_error_->setText(message);
    label_error_->setVisible(true);
}
