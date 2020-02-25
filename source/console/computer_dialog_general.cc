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

#include "console/computer_dialog_general.h"

#include "base/strings/unicode.h"
#include "common/user_util.h"
#include "net/address.h"

#include <QMessageBox>

namespace console {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialogGeneral::ComputerDialogGeneral(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerDialogGeneral::showPasswordButtonToggled);

    connect(ui.edit_address, &QLineEdit::textEdited, [this](const QString& text)
    {
        if (!has_name_)
            ui.edit_name->setText(text);
    });

    connect(ui.edit_name, &QLineEdit::textEdited, [this](const QString& text)
    {
        has_name_ = !text.isEmpty();
    });

    ui.edit_address->setFocus();
}

void ComputerDialogGeneral::restoreSettings(
    const QString& parent_name, const proto::address_book::Computer& computer)
{
    net::Address address;
    address.setHost(base::utf16FromUtf8(computer.address()));
    address.setPort(computer.port());

    ui.edit_parent_name->setText(parent_name);
    ui.edit_name->setText(QString::fromStdString(computer.name()));
    ui.edit_address->setText(QString::fromStdU16String(address.toString()));
    ui.edit_username->setText(QString::fromStdString(computer.username()));
    ui.edit_password->setText(QString::fromStdString(computer.password()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer.comment()));

    has_name_ = !computer.name().empty();
    if (has_name_)
        ui.edit_name->setFocus();
}

bool ComputerDialogGeneral::saveSettings(proto::address_book::Computer* computer)
{
    QString name = ui.edit_name->text();
    if (name.length() > kMaxNameLength)
    {
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", kMaxNameLength));
        ui.edit_name->setFocus();
        ui.edit_name->selectAll();
        return false;
    }
    else if (name.length() < kMinNameLength)
    {
        showError(tr("Name can not be empty."));
        ui.edit_name->setFocus();
        return false;
    }

    std::u16string username = ui.edit_username->text().toStdU16String();
    std::u16string password = ui.edit_password->text().toStdU16String();

    if (!username.empty() && !common::UserUtil::isValidUserName(username))
    {
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui.edit_name->setFocus();
        ui.edit_name->selectAll();
        return false;
    }

    QString comment = ui.edit_comment->toPlainText();
    if (comment.length() > kMaxCommentLength)
    {
        showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                     "", kMaxCommentLength));
        ui.edit_comment->setFocus();
        ui.edit_comment->selectAll();
        return false;
    }

    net::Address address = net::Address::fromString(ui.edit_address->text().toStdU16String());
    if (!address.isValid())
    {
        showError(tr("An invalid computer address was entered."));
        ui.edit_address->setFocus();
        ui.edit_address->selectAll();
        return false;
    }

    computer->set_name(name.toStdString());
    computer->set_address(base::utf8FromUtf16(address.host()));
    computer->set_port(address.port());
    computer->set_username(base::utf8FromUtf16(username));
    computer->set_password(base::utf8FromUtf16(password));
    computer->set_comment(comment.toStdString());
    return true;
}

void ComputerDialogGeneral::showPasswordButtonToggled(bool checked)
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

void ComputerDialogGeneral::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
