//
// Aspia Project
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

#include "console/computer_group_dialog_general.h"

#include "base/logging.h"
#include "base/peer/user.h"

#include <QMessageBox>
#include <QTimer>

namespace console {

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogGeneral::ComputerGroupDialogGeneral(int type, bool is_root_group, QWidget* parent)
    : ComputerGroupDialogTab(type, is_root_group, parent)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerGroupDialogGeneral::showPasswordButtonToggled);

    if (is_root_group)
    {
        ui.groupbox_inherit_creds->setCheckable(false);
        ui.groupbox_inherit_creds->setTitle(tr("Credentials"));
    }
    else
    {
        ui.groupbox_inherit_creds->setCheckable(true);
        ui.groupbox_inherit_creds->setTitle(tr("Inherit from parent"));

        connect(ui.groupbox_inherit_creds, &QGroupBox::toggled, this, [this](bool checked)
        {
            QWidgetList widgets = ui.groupbox_inherit_creds->findChildren<QWidget*>();
            for (const auto& widget : std::as_const(widgets))
            {
                widget->setEnabled(!checked);
            }
        });
    }

    ui.edit_username->setFocus();
}

//--------------------------------------------------------------------------------------------------
ComputerGroupDialogGeneral::~ComputerGroupDialogGeneral()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogGeneral::restoreSettings(
    const proto::address_book::ComputerGroupConfig& group_config)
{
    ui.edit_username->setText(QString::fromStdString(group_config.username()));
    ui.edit_password->setText(QString::fromStdString(group_config.password()));

    if (!isRootGroup())
    {
        bool inherit_creds = group_config.inherit().credentials();

        ui.groupbox_inherit_creds->setChecked(inherit_creds);
        QTimer::singleShot(0, this, [this, inherit_creds]()
        {
            QWidgetList widgets = ui.groupbox_inherit_creds->findChildren<QWidget*>();
            for (const auto& widget : std::as_const(widgets))
            {
                widget->setEnabled(!inherit_creds);
            }
        });
    }
}

//--------------------------------------------------------------------------------------------------
bool ComputerGroupDialogGeneral::saveSettings(proto::address_book::ComputerGroupConfig* group_config)
{
    QString username = ui.edit_username->text();
    QString password = ui.edit_password->text();

    if (!username.isEmpty() && !base::User::isValidUserName(username))
    {
        LOG(LS_INFO) << "Invalid user name:" << username;
        showError(tr("The user name can not be empty and can contain only"
                     " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
        ui.edit_username->setFocus();
        ui.edit_username->selectAll();
        return false;
    }

    group_config->set_username(username.toStdString());
    group_config->set_password(password.toStdString());

    if (!isRootGroup())
    {
        group_config->mutable_inherit()->set_credentials(ui.groupbox_inherit_creds->isChecked());
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void ComputerGroupDialogGeneral::showPasswordButtonToggled(bool checked)
{
    LOG(LS_INFO) << "[ACTION] Show password:" << checked;
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
void ComputerGroupDialogGeneral::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace console
