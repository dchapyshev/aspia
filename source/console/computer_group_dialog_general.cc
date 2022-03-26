//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "console/computer_group_dialog_general.h"

#include "base/net/address.h"
#include "base/peer/user.h"
#include "base/strings/unicode.h"

#include <QMessageBox>

namespace console {

ComputerGroupDialogGeneral::ComputerGroupDialogGeneral(int type, QWidget* parent)
    : ComputerGroupDialogTab(type, parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerGroupDialogGeneral::showPasswordButtonToggled);

    ui.edit_username->setFocus();
}

void ComputerGroupDialogGeneral::restoreSettings(const proto::address_book::Computer& computer)
{
    ui.edit_username->setText(QString::fromStdString(computer.username()));
    ui.edit_password->setText(QString::fromStdString(computer.password()));
}

bool ComputerGroupDialogGeneral::saveSettings(proto::address_book::Computer* computer)
{
    std::u16string username = ui.edit_username->text().toStdU16String();
    std::u16string password = ui.edit_password->text().toStdU16String();

    if (!username.empty() && !base::User::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui.edit_username->setFocus();
        ui.edit_username->selectAll();
        return false;
    }

    computer->set_username(base::utf8FromUtf16(username));
    computer->set_password(base::utf8FromUtf16(password));
    return true;
}

void ComputerGroupDialogGeneral::showPasswordButtonToggled(bool checked)
{
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

void ComputerGroupDialogGeneral::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
