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

#include "client/desktop/authorization_dialog.h"

#include <QComboBox>
#include <QIcon>
#include <QRadioButton>
#include <QTimer>

#include "base/logging.h"
#include "base/time_types.h"
#include "base/crypto/secure_string.h"
#include "client/settings.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "ui_authorization_dialog.h"

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::AuthorizationDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::AuthorizationDialog>()),
      one_time_password_choice_(Settings().isOneTimePasswordChecked())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->checkbox_one_time_password->setChecked(one_time_password_choice_);
    onOneTimePasswordToggled(one_time_password_choice_);

    ui->edit_password->setShowPasswordButtonVisible(true);

    connect(ui->buttonbox, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);

    // The box follows the state of the dialog, so what it holds is not always an answer of the
    // user. Only clicked() is one, and only that is remembered.
    connect(ui->checkbox_one_time_password, &QCheckBox::toggled,
            this, &AuthorizationDialog::onOneTimePasswordToggled);
    connect(ui->checkbox_one_time_password, &QCheckBox::clicked,
            this, &AuthorizationDialog::onOneTimePasswordClicked);
    connect(ui->radio_shared, &QRadioButton::toggled,
            this, &AuthorizationDialog::onSharedToggled);

    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::~AuthorizationDialog()
{
    LOG(INFO) << "Dtor";

    Settings settings;
    settings.setOneTimePasswordChecked(one_time_password_choice_);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setOneTimePasswordEnabled(bool enable)
{
    one_time_password_enabled_ = enable;
    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setCredentials(const QList<CredentialConfig>& credentials)
{
    credentials_ = credentials;

    ui->combo_credential->clear();
    for (const CredentialConfig& credential : std::as_const(credentials_))
    {
        ui->combo_credential->addItem(QIcon(":/img/keys.svg"), credential.displayName(),
                                      QVariant::fromValue(credential.id()));
    }

    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
qint64 AuthorizationDialog::credentialId() const
{
    const CredentialConfig* credential = selectedCredential();
    return credential ? credential->id() : 0;
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setSaveCredentialsVisible(bool visible)
{
    save_credentials_visible_ = visible;
    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isSaveCredentialsChecked() const
{
    // What the user was never shown is not his answer.
    return isSaveCredentialsOffered() && ui->checkbox_save_credentials->isChecked();
}

//--------------------------------------------------------------------------------------------------
QString AuthorizationDialog::userName() const
{
    // A one-time password is asked of the host itself, and the field is hidden while it is. What
    // the user was never shown is not his answer.
    if (isOneTimePassword())
        return QString();

    const CredentialConfig* credential = selectedCredential();
    if (credential)
        return credential->username();

    return ui->edit_username->text();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setUserName(const QString& username)
{
    ui->edit_username->setText(username);

    // A saved user name means a named-user connection, so the one-time password path does not apply.
    if (!username.isEmpty())
        ui->checkbox_one_time_password->setChecked(false);
}

//--------------------------------------------------------------------------------------------------
SecureString AuthorizationDialog::password() const
{
    const CredentialConfig* credential = selectedCredential();
    if (credential)
        return credential->password();

    return ui->edit_password->password();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setPassword(const SecureString& password)
{
    ui->edit_password->setPassword(password);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::showEvent(QShowEvent* event)
{
    LOG(INFO) << "Show event detected";

    if (isShared())
        ui->combo_credential->setFocus();
    else if (ui->edit_username->text().isEmpty() && !isOneTimePassword())
        ui->edit_username->setFocus();
    else
        ui->edit_password->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onSharedToggled(bool checked)
{
    LOG(INFO) << "[ACTION] Saved credentials chosen:" << checked;
    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onOneTimePasswordToggled(bool checked)
{
    LOG(INFO) << "[ACTION] One time password toggled:" << checked;

    if (checked)
        ui->edit_username->clear();

    updateCredentialsState();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onOneTimePasswordClicked(bool checked)
{
    one_time_password_choice_ = checked;
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonbox->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (!isShared())
        {
            if (!isOneTimePassword() && ui->edit_username->text().isEmpty())
            {
                LOG(ERROR) << "Empty user name";
                MsgBox::warning(this, tr("User name cannot be empty."));
                return;
            }

            if (ui->edit_password->password().isEmpty())
            {
                LOG(ERROR) << "Empty password";
                MsgBox::warning(this, tr("Password cannot be empty."));
                return;
            }
        }

        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::fitSize()
{
    QTimer::singleShot(MilliSeconds(0), this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isOneTimePassword() const
{
    return one_time_password_enabled_ && ui->checkbox_one_time_password->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isSaveCredentialsOffered() const
{
    return save_credentials_visible_ && !isOneTimePassword();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isSharedOffered() const
{
    return !credentials_.isEmpty() && !isOneTimePassword();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isShared() const
{
    return isSharedOffered() && ui->radio_shared->isChecked();
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* AuthorizationDialog::selectedCredential() const
{
    if (!isShared())
        return nullptr;

    const qint64 credential_id = ui->combo_credential->currentData().toLongLong();
    for (const CredentialConfig& credential : std::as_const(credentials_))
    {
        if (credential.id() == credential_id)
            return &credential;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::updateCredentialsState()
{
    const bool shared_offered = isSharedOffered();

    ui->radio_shared->setEnabled(shared_offered);

    if (!shared_offered && ui->radio_shared->isChecked())
    {
        const QSignalBlocker blocker(ui->radio_shared);
        ui->radio_typed->setChecked(true);
    }

    const bool shared = isShared();
    const bool one_time = isOneTimePassword();

    ui->checkbox_one_time_password->setVisible(one_time_password_enabled_);
    ui->checkbox_one_time_password->setEnabled(!shared);

    ui->label_username->setVisible(!one_time);
    ui->edit_username->setVisible(!one_time);

    ui->label_username->setEnabled(!shared);
    ui->edit_username->setEnabled(!shared);
    ui->label_password->setEnabled(!shared);
    ui->edit_password->setEnabled(!shared);

    ui->label_credential->setEnabled(shared);
    ui->combo_credential->setEnabled(shared);

    ui->checkbox_save_credentials->setVisible(isSaveCredentialsOffered());

    fitSize();
}
