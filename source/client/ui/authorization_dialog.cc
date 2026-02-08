//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/authorization_dialog.h"

#include "client/ui/client_settings.h"
#include "base/logging.h"

#include <QMessageBox>
#include <QTimer>

namespace client {

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::AuthorizationDialog(QWidget* parent)
    : QDialog(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    QPushButton* cancel_button = ui.buttonbox->button(QDialogButtonBox::StandardButton::Cancel);
    if (cancel_button)
        cancel_button->setText(tr("Cancel"));

    ClientSettings settings;

    bool is_one_time_password_checked = settings.isOneTimePasswordChecked();
    ui.checkbox_one_time_password->setChecked(is_one_time_password_checked);
    onOneTimePasswordToggled(is_one_time_password_checked);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &AuthorizationDialog::onShowPasswordButtonToggled);

    connect(ui.buttonbox, &QDialogButtonBox::clicked,
            this, &AuthorizationDialog::onButtonBoxClicked);

    connect(ui.checkbox_one_time_password, &QCheckBox::toggled,
            this, &AuthorizationDialog::onOneTimePasswordToggled);

    fitSize();
}

//--------------------------------------------------------------------------------------------------
AuthorizationDialog::~AuthorizationDialog()
{
    LOG(INFO) << "Dtor";

    ClientSettings settings;
    settings.setOneTimePasswordChecked(ui.checkbox_one_time_password->isChecked());
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setOneTimePasswordEnabled(bool enable)
{
    ui.checkbox_one_time_password->setVisible(enable);

    if (!enable)
    {
        ui.label_username->setVisible(true);
        ui.edit_username->setVisible(true);
    }
    else
    {
        bool is_checked = ui.checkbox_one_time_password->isChecked();

        ui.label_username->setVisible(!is_checked);
        ui.edit_username->setVisible(!is_checked);
    }

    fitSize();
}

//--------------------------------------------------------------------------------------------------
QString AuthorizationDialog::userName() const
{
    return ui.edit_username->text();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setUserName(const QString& username)
{
    ui.edit_username->setText(username);
}

//--------------------------------------------------------------------------------------------------
QString AuthorizationDialog::password() const
{
    return ui.edit_password->text();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::setPassword(const QString& password)
{
    ui.edit_password->setText(password);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::showEvent(QShowEvent* event)
{
    LOG(INFO) << "Show event detected";

    if (ui.edit_username->text().isEmpty() && !ui.checkbox_one_time_password->isChecked())
        ui.edit_username->setFocus();
    else
        ui.edit_password->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onShowPasswordButtonToggled(bool checked)
{
    LOG(INFO) << "[ACTION] Show passowrd button toggled:" << checked;

    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }

    ui.edit_password->setFocus();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onOneTimePasswordToggled(bool checked)
{
    LOG(INFO) << "[ACTION] One time password toggled:" << checked;

    ui.label_username->setVisible(!checked);
    ui.edit_username->setVisible(!checked);
    ui.edit_username->clear();

    fitSize();
}

//--------------------------------------------------------------------------------------------------
void AuthorizationDialog::onButtonBoxClicked(QAbstractButton* button)
{
    if (ui.buttonbox->standardButton(button) == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (!ui.checkbox_one_time_password->isChecked())
        {
            if (ui.edit_username->text().isEmpty())
            {
                LOG(ERROR) << "Empty user name";
                QMessageBox::warning(this,
                                     tr("Warning"),
                                     tr("Username cannot be empty."),
                                     QMessageBox::Ok);
                return;
            }
        }

        if (ui.edit_password->text().isEmpty())
        {
            LOG(ERROR) << "Empty password";
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Password cannot be empty."),
                                 QMessageBox::Ok);
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
    QTimer::singleShot(0, this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

} // namespace client
