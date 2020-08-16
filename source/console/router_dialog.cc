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

#include "console/router_dialog.h"

#include "base/guid.h"
#include "base/net/address.h"
#include "base/peer/user.h"
#include "base/strings/unicode.h"

#include <QAbstractButton>
#include <QMessageBox>

namespace console {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

RouterDialog::RouterDialog(
    const std::optional<proto::address_book::Router>& router, QWidget* parent)
    : QDialog(parent),
      router_(router)
{
    ui.setupUi(this);

    if (router_.has_value())
    {
        base::Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(base::utf16FromUtf8(router_->address()));
        address.setPort(router_->port());

        ui.edit_name->setText(QString::fromStdString(router_->name()));
        ui.edit_address->setText(QString::fromStdU16String(address.toString()));
        ui.edit_username->setText(QString::fromStdString(router_->username()));
        ui.edit_password->setText(QString::fromStdString(router_->password()));
        ui.edit_comment->setPlainText(QString::fromStdString(router_->comment()));
    }

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &RouterDialog::showPasswordButtonToggled);
    connect(ui.buttonbox, &QDialogButtonBox::clicked, this, &RouterDialog::onButtonBoxClicked);

    ui.edit_name->setFocus();
}

RouterDialog::~RouterDialog() = default;

void RouterDialog::showPasswordButtonToggled(bool checked)
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

void RouterDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        reject();
    }
    else
    {
        QString name = ui.edit_name->text();
        if (name.length() > kMaxNameLength)
        {
            showError(tr("Too long name. The maximum length of the name is %n characters.",
                         "", kMaxNameLength));
            ui.edit_name->setFocus();
            ui.edit_name->selectAll();
            return;
        }
        else if (name.length() < kMinNameLength)
        {
            showError(tr("Name can not be empty."));
            ui.edit_name->setFocus();
            return;
        }

        std::u16string username = ui.edit_username->text().toStdU16String();
        std::u16string password = ui.edit_password->text().toStdU16String();

        if (!username.empty() && !base::User::isValidUserName(username))
        {
            showError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            ui.edit_name->setFocus();
            ui.edit_name->selectAll();
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                         "", kMaxCommentLength));
            ui.edit_comment->setFocus();
            ui.edit_comment->selectAll();
            return;
        }

        base::Address address = base::Address::fromString(
            ui.edit_address->text().toStdU16String(), DEFAULT_ROUTER_TCP_PORT);
        if (!address.isValid())
        {
            showError(tr("An invalid router address was entered."));
            ui.edit_address->setFocus();
            ui.edit_address->selectAll();
            return;
        }

        if (!router_.has_value())
        {
            router_ = proto::address_book::Router();
            router_->set_guid(base::Guid::create().toStdString());
        }

        router_->set_name(name.toStdString());
        router_->set_address(base::utf8FromUtf16(address.host()));
        router_->set_port(address.port());
        router_->set_username(base::utf8FromUtf16(username));
        router_->set_password(base::utf8FromUtf16(password));
        router_->set_comment(comment.toStdString());

        accept();
    }

    close();
}

void RouterDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
