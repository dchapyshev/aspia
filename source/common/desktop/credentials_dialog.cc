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

#include "common/desktop/credentials_dialog.h"

#include <QAbstractButton>
#include <QShowEvent>
#include <QTimer>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/password_edit.h"
#include "ui_credentials_dialog.h"

//--------------------------------------------------------------------------------------------------
CredentialsDialog::CredentialsDialog(Type type, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::CredentialsDialog>()),
      type_(type)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->label_icon->setVisible(false);
    ui->label_description->setVisible(false);

    switch (type_)
    {
        case Type::ENTER_PASSWORD:
            ui->label_current->setVisible(false);
            ui->edit_current->setVisible(false);
            ui->label_confirm->setVisible(false);
            ui->edit_confirm->setVisible(false);
            ui->label_password->setText(tr("Password:"));
            break;

        case Type::SET_PASSWORD:
            ui->label_current->setVisible(false);
            ui->edit_current->setVisible(false);
            ui->label_password->setText(tr("New password:"));
            break;

        case Type::CHANGE_PASSWORD:
            ui->label_password->setText(tr("New password:"));
            break;
    }

    connect(ui->button_box, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button)
    {
        if (ui->button_box->standardButton(button) != QDialogButtonBox::Ok)
        {
            reject();
            return;
        }

        if (!defaultValidate())
            return;

        if (validator_ && !validator_(this))
            return;

        accept();
    });

    fitHeight();
}

//--------------------------------------------------------------------------------------------------
CredentialsDialog::~CredentialsDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setHeaderIcon(const QString& svg_icon_path)
{
    bool has_icon = !svg_icon_path.isEmpty();

    if (has_icon)
        ui->label_icon->setPixmap(GuiApplication::svgPixmap(svg_icon_path, QSize(48, 48)));
    ui->label_icon->setVisible(has_icon);

    fitHeight();
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setHeaderText(const QString& text)
{
    bool has_text = !text.isEmpty();

    if (has_text)
        ui->label_description->setText(text);
    ui->label_description->setVisible(has_text);

    fitHeight();
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setShowPasswordButtonVisible(bool visible)
{
    ui->edit_current->setShowPasswordButtonVisible(visible);
    ui->edit_password->setShowPasswordButtonVisible(visible);
    ui->edit_confirm->setShowPasswordButtonVisible(visible);
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setPasswordLabel(const QString& text)
{
    if (type_ == Type::ENTER_PASSWORD)
        ui->label_password->setText(text);
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setNewPasswordLabel(const QString& text)
{
    if (type_ != Type::ENTER_PASSWORD)
        ui->label_password->setText(text);
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setConfirmPasswordLabel(const QString& text)
{
    ui->label_confirm->setText(text);
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setCurrentPasswordLabel(const QString& text)
{
    ui->label_current->setText(text);
}

//--------------------------------------------------------------------------------------------------
SecureString CredentialsDialog::currentPassword() const
{
    return type_ == Type::CHANGE_PASSWORD ? ui->edit_current->password() : SecureString();
}

//--------------------------------------------------------------------------------------------------
SecureString CredentialsDialog::password() const
{
    return ui->edit_password->password();
}

//--------------------------------------------------------------------------------------------------
SecureString CredentialsDialog::confirmPassword() const
{
    return type_ != Type::ENTER_PASSWORD ? ui->edit_confirm->password() : SecureString();
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::setValidator(Validator validator)
{
    validator_ = std::move(validator);
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::showEvent(QShowEvent* event)
{
    PasswordEdit* first_empty = nullptr;
    if (type_ == Type::CHANGE_PASSWORD && ui->edit_current->password().isEmpty())
        first_empty = ui->edit_current;
    else if (ui->edit_password->password().isEmpty())
        first_empty = ui->edit_password;
    else if (type_ != Type::ENTER_PASSWORD && ui->edit_confirm->password().isEmpty())
        first_empty = ui->edit_confirm;

    if (first_empty)
        first_empty->setFocus();

    QDialog::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
bool CredentialsDialog::defaultValidate()
{
    if (type_ == Type::CHANGE_PASSWORD && ui->edit_current->password().isEmpty())
    {
        MsgBox::warning(this, tr("Enter the current password."));
        ui->edit_current->setFocus();
        return false;
    }

    if (ui->edit_password->password().isEmpty())
    {
        MsgBox::warning(this, tr("Password cannot be empty."));
        ui->edit_password->setFocus();
        return false;
    }

    if (type_ != Type::ENTER_PASSWORD && ui->edit_password->password() != ui->edit_confirm->password())
    {
        MsgBox::warning(this, tr("Passwords do not match."));
        ui->edit_confirm->setFocus();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void CredentialsDialog::fitHeight()
{
    QTimer::singleShot(0, this, [this]() { setFixedHeight(sizeHint().height()); });
}
