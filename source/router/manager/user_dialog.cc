//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/user_dialog.h"

#include "common/user_util.h"

#include <QAbstractButton>
#include <QMessageBox>

namespace router {

UserDialog::UserDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &UserDialog::onButtonBoxClicked);
}

UserDialog::~UserDialog() = default;

void UserDialog::setUserName(const QString& username)
{
    ui.edit_username->setText(username);
}

QString UserDialog::userName() const
{
    return ui.edit_username->text();
}

QString UserDialog::password() const
{
    return ui.edit_password->text();
}

void UserDialog::setEnabled(bool enable)
{
    ui.checkbox_enable->setChecked(enable);
}

bool UserDialog::isEnabled() const
{
    return ui.checkbox_enable->isChecked();
}

void UserDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        std::u16string name = userName().toStdU16String();

        if (!common::UserUtil::isValidUserName(name))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The user name can not be empty and can contain only alphabet"
                                    " characters, numbers and ""_"", ""-"", ""."" characters."),
                                 QMessageBox::Ok);

            ui.edit_username->selectAll();
            ui.edit_username->setFocus();
            return;
        }

        if (ui.edit_password->text() != ui.edit_password_retry->text())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("The passwords you entered do not match."),
                                 QMessageBox::Ok);

            ui.edit_password->selectAll();
            ui.edit_password->setFocus();
            return;
        }

        std::u16string pass = password().toStdU16String();

        if (!common::UserUtil::isValidPassword(pass))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Password can not be empty and should not exceed %n characters.",
                                    "", common::UserUtil::kMaxPasswordLength),
                                 QMessageBox::Ok);

            ui.edit_password->selectAll();
            ui.edit_password->setFocus();
            return;
        }

        if (!common::UserUtil::isSafePassword(pass))
        {
            QString unsafe =
                tr("Password you entered does not meet the security requirements!");

            QString safe =
                tr("The password must contain lowercase and uppercase characters, "
                   "numbers and should not be shorter than %n characters.",
                   "", common::UserUtil::kSafePasswordLength);

            QString question = tr("Do you want to enter a different password?");

            if (QMessageBox::warning(this,
                                     tr("Warning"),
                                     QString("<b>%1</b><br/>%2<br/>%3")
                                     .arg(unsafe).arg(safe).arg(question),
                                     QMessageBox::Yes,
                                     QMessageBox::No) == QMessageBox::Yes)
            {
                ui.edit_password->clear();
                ui.edit_password_retry->clear();
                ui.edit_password->setFocus();
                return;
            }
        }

        accept();
    }
    else
    {
        reject();
    }

    close();
}

} // namespace router
