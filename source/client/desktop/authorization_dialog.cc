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
      ui(std::make_unique<Ui::AuthorizationDialog>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    Settings settings;

    bool is_one_time_password_checked = settings.isOneTimePasswordChecked();
    ui->checkbox_one_time_password->setChecked(is_one_time_password_checked);
    onOneTimePasswordToggled(is_one_time_password_checked);

    ui->edit_password->setShowPasswordButtonVisible(true);

    connect(ui->buttonbox, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);

    connect(ui->checkbox_one_time_password, &QCheckBox::toggled,
            this, &AuthorizationDialog::onOneTimePasswordToggled);

    fitSize();
}

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::~AuthorizationDialog()
{
    LOG(INFO) << "Dtor";

    Settings settings;
    settings.setOneTimePasswordChecked(ui->checkbox_one_time_password->isChecked());
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setOneTimePasswordEnabled(bool enable)
{
    one_time_password_enabled_ = enable;

    ui->checkbox_one_time_password->setVisible(enable);

    const bool one_time = isOneTimePassword();

    ui->label_username->setVisible(!one_time);
    ui->edit_username->setVisible(!one_time);
    ui->checkbox_save_credentials->setVisible(isSaveCredentialsOffered());

    fitSize();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setSaveCredentialsVisible(bool visible)
{
    save_credentials_visible_ = visible;

    ui->checkbox_save_credentials->setVisible(isSaveCredentialsOffered());

    fitSize();
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

    if (ui->edit_username->text().isEmpty() && !isOneTimePassword())
        ui->edit_username->setFocus();
    else
        ui->edit_password->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onOneTimePasswordToggled(bool checked)
{
    LOG(INFO) << "[ACTION] One time password toggled:" << checked;

    ui->label_username->setVisible(!checked);
    ui->edit_username->setVisible(!checked);

    if (checked)
        ui->edit_username->clear();

    // A one-time password is good for one connection, so there is nothing to keep.
    ui->checkbox_save_credentials->setVisible(isSaveCredentialsOffered());

    fitSize();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonbox->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (!isOneTimePassword())
        {
            if (ui->edit_username->text().isEmpty())
            {
                LOG(ERROR) << "Empty user name";
                MsgBox::warning(this, tr("Username cannot be empty."));
                return;
            }
        }

        if (ui->edit_password->password().isEmpty())
        {
            LOG(ERROR) << "Empty password";
            MsgBox::warning(this, tr("Password cannot be empty."));
            return;
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
