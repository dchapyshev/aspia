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

#include "console/computer_dialog_general.h"

#include <QMessageBox>
#include <QTimer>

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"
#include "base/peer/user.h"
#include "build/build_config.h"

namespace console {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

//--------------------------------------------------------------------------------------------------
ComputerDialogGeneral::ComputerDialogGeneral(int type, QWidget* parent)
    : ComputerDialogTab(type, parent)
{
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerDialogGeneral::showPasswordButtonToggled);

    connect(ui.edit_address, &QLineEdit::textEdited, this, [this](const QString& text)
    {
        if (!has_name_)
            ui.edit_name->setText(text);
    });

    connect(ui.edit_name, &QLineEdit::textEdited, this, [this](const QString& text)
    {
        has_name_ = !text.isEmpty();
    });

    connect(ui.groupbox_inherit_creds, &QGroupBox::toggled, this, [this](bool checked)
    {
        QWidgetList widgets = ui.groupbox_inherit_creds->findChildren<QWidget*>();
        for (const auto& widget : std::as_const(widgets))
        {
            widget->setEnabled(!checked);
        }
    });

    ui.edit_address->setFocus();
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogGeneral::restoreSettings(const QString& parent_name,
                                            const proto::address_book::Computer& computer)
{
    bool inherit_creds = computer.inherit().credentials();

    ui.groupbox_inherit_creds->setChecked(inherit_creds);
    QTimer::singleShot(0, this, [this, inherit_creds]()
    {
        QWidgetList widgets = ui.groupbox_inherit_creds->findChildren<QWidget*>();
        for (const auto& widget : std::as_const(widgets))
        {
            widget->setEnabled(!inherit_creds);
        }
    });

    if (base::isHostId(QString::fromStdString(computer.address())))
    {
        ui.edit_address->setText(QString::fromStdString(computer.address()));
    }
    else
    {
        base::Address address(DEFAULT_HOST_TCP_PORT);
        address.setHost(QString::fromStdString(computer.address()));
        address.setPort(static_cast<quint16>(computer.port()));

        ui.edit_address->setText(address.toString());
    }

    ui.edit_parent_name->setText(parent_name);
    ui.edit_name->setText(QString::fromStdString(computer.name()));
    ui.edit_username->setText(QString::fromStdString(computer.username()));
    ui.edit_password->setText(QString::fromStdString(computer.password()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer.comment()));

    has_name_ = !computer.name().empty();
    if (!has_name_)
        ui.edit_name->setFocus();
}

//--------------------------------------------------------------------------------------------------
bool ComputerDialogGeneral::saveSettings(proto::address_book::Computer* computer)
{
    QString name = ui.edit_name->text();
    if (name.length() > kMaxNameLength)
    {
        LOG(ERROR) << "Too long name:" << name.length();
        showError(tr("Too long name. The maximum length of the name is %n characters.",
                     "", kMaxNameLength));
        ui.edit_name->setFocus();
        ui.edit_name->selectAll();
        return false;
    }
    else if (name.length() < kMinNameLength)
    {
        LOG(ERROR) << "Name can not be empty";
        showError(tr("Name can not be empty."));
        ui.edit_name->setFocus();
        return false;
    }

    QString username = ui.edit_username->text();
    QString password = ui.edit_password->text();

    if (!username.isEmpty() && !base::User::isValidUserName(username))
    {
        LOG(ERROR) << "Invalid user name:" << username;
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui.edit_username->setFocus();
        ui.edit_username->selectAll();
        return false;
    }

    QString comment = ui.edit_comment->toPlainText();
    if (comment.length() > kMaxCommentLength)
    {
        LOG(ERROR) << "Too long comment:" << comment.length();
        showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                     "", kMaxCommentLength));
        ui.edit_comment->setFocus();
        ui.edit_comment->selectAll();
        return false;
    }

    if (base::isHostId(ui.edit_address->text()))
    {
        computer->set_address(ui.edit_address->text().toStdString());
    }
    else
    {
        base::Address address = base::Address::fromString(
            ui.edit_address->text(), DEFAULT_HOST_TCP_PORT);
        if (!address.isValid())
        {
            LOG(ERROR) << "Invalid address:" << ui.edit_address->text();
            showError(tr("An invalid computer address was entered."));
            ui.edit_address->setFocus();
            ui.edit_address->selectAll();
            return false;
        }

        computer->set_address(address.host().toStdString());
        computer->set_port(address.port());
    }

    computer->mutable_inherit()->set_credentials(ui.groupbox_inherit_creds->isChecked());
    computer->set_name(name.toStdString());
    computer->set_username(username.toStdString());
    computer->set_password(password.toStdString());
    computer->set_comment(comment.toStdString());
    return true;
}

//--------------------------------------------------------------------------------------------------
void ComputerDialogGeneral::showPasswordButtonToggled(bool checked)
{
    LOG(INFO) << "[ACTION] Show password:" << checked;
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
void ComputerDialogGeneral::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
