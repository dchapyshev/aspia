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
#include "client/desktop/ui_authorization_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::AuthorizationDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::AuthorizationDialog>()),
      one_time_password_choice_(Settings().isOneTimePasswordChecked())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_password->setShowPasswordButtonVisible(true);
    ui->edit_one_time_password->setShowPasswordButtonVisible(true);

    connect(ui->buttonbox, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);

    // A button follows the state of the dialog, so what it holds is not always an answer of the
    // user. Only clicked() is one, and only that is remembered.
    for (QRadioButton* button : { ui->radio_user_password, ui->radio_one_time_password, ui->radio_saved_credentials })
    {
        connect(button, &QRadioButton::toggled, this, &AuthorizationDialog::onModeToggled);
        connect(button, &QRadioButton::clicked, this, &AuthorizationDialog::onModeClicked);
    }

    updateModes();
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

    if (enable && one_time_password_choice_)
        ui->radio_one_time_password->setChecked(true);

    updateModes();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setSavedCredentials(const QList<CredentialConfig>& credentials)
{
    credentials_.clear();
    ui->combo_credential->clear();

    const QIcon icon(":/img/keys.svg");

    // A record that did not open hands over an empty user name and password. Offered, it
    // would connect with nothing and the host would refuse credentials the user never saw.
    for (const CredentialConfig& credential : credentials)
    {
        if (!credential.isValid())
            continue;

        credentials_.append(credential);
        ui->combo_credential->addItem(icon, credential.displayName(), QVariant::fromValue(credential.id()));
    }

    updateModes();
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
    updateModes();
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
    // A one-time password is the host naming itself, so the connection carries no user name.
    if (usesOneTimePassword())
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
        ui->radio_user_password->setChecked(true);
}

//--------------------------------------------------------------------------------------------------
SecureString AuthorizationDialog::password() const
{
    if (usesOneTimePassword())
        return ui->edit_one_time_password->password();

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

    if (usesSavedCredentials())
        ui->combo_credential->setFocus();
    else if (usesOneTimePassword())
        ui->edit_one_time_password->setFocus();
    else if (ui->edit_username->text().isEmpty())
        ui->edit_username->setFocus();
    else
        ui->edit_password->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onModeToggled(bool /* checked */)
{
    updateModes();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onModeClicked(bool /* checked */)
{
    one_time_password_choice_ = ui->radio_one_time_password->isChecked();
    LOG(INFO) << "[ACTION] One time password chosen:" << one_time_password_choice_;
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonbox->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (usesOneTimePassword())
        {
            if (ui->edit_one_time_password->password().isEmpty())
            {
                LOG(ERROR) << "Empty password";
                MsgBox::warning(this, tr("Password cannot be empty."));
                return;
            }
        }
        else if (!usesSavedCredentials())
        {
            if (ui->edit_username->text().isEmpty())
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
bool AuthorizationDialog::usesOneTimePassword() const
{
    return isOneTimePasswordOffered() && ui->radio_one_time_password->isChecked();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isOneTimePasswordOffered() const
{
    return one_time_password_enabled_;
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::isSaveCredentialsOffered() const
{
    return save_credentials_visible_ && !usesOneTimePassword();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::hasSavedCredentials() const
{
    return !credentials_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool AuthorizationDialog::usesSavedCredentials() const
{
    return hasSavedCredentials() && ui->radio_saved_credentials->isChecked();
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* AuthorizationDialog::selectedCredential() const
{
    if (!usesSavedCredentials())
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
void AuthorizationDialog::updateModes()
{
    const bool one_time_offered = isOneTimePasswordOffered();
    const bool saved_credentials_offered = hasSavedCredentials();

    if ((!one_time_offered && ui->radio_one_time_password->isChecked()) ||
        (!saved_credentials_offered && ui->radio_saved_credentials->isChecked()))
    {
        const QSignalBlocker one_time_password_blocker(ui->radio_one_time_password);
        const QSignalBlocker saved_credentials_blocker(ui->radio_saved_credentials);
        ui->radio_user_password->setChecked(true);
    }

    const bool has_choice = one_time_offered || saved_credentials_offered;

    ui->radio_user_password->setVisible(has_choice);
    ui->radio_one_time_password->setVisible(one_time_offered);
    ui->radio_saved_credentials->setVisible(saved_credentials_offered);

    ui->label_one_time_password->setVisible(one_time_offered);
    ui->edit_one_time_password->setVisible(one_time_offered);
    ui->label_credential->setVisible(saved_credentials_offered);
    ui->combo_credential->setVisible(saved_credentials_offered);

    const bool one_time = usesOneTimePassword();
    const bool saved = usesSavedCredentials();
    const bool user_password = !one_time && !saved;

    ui->label_username->setEnabled(user_password);
    ui->edit_username->setEnabled(user_password);
    ui->label_password->setEnabled(user_password);
    ui->edit_password->setEnabled(user_password);

    ui->label_one_time_password->setEnabled(one_time);
    ui->edit_one_time_password->setEnabled(one_time);

    ui->label_credential->setEnabled(saved);
    ui->combo_credential->setEnabled(saved);

    ui->checkbox_save_credentials->setVisible(isSaveCredentialsOffered());

    fitSize();
}
