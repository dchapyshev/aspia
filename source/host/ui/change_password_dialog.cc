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

#include "host/ui/change_password_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QTimer>

#include "common/ui/msg_box.h"
#include "base/logging.h"
#include "host/system_settings.h"

namespace host {

//--------------------------------------------------------------------------------------------------
ChangePasswordDialog::ChangePasswordDialog(Mode mode, QWidget* parent)
    : QDialog(parent),
      mode_(mode)
{
    LOG(INFO) << "Ctor (" << static_cast<int>(mode) << ")";
    ui.setupUi(this);

    if (mode == Mode::CREATE_NEW_PASSWORD)
    {
        ui.label_old_pass->setVisible(false);
        ui.edit_old_pass->setVisible(false);
    }
    else
    {
        DCHECK_EQ(mode, Mode::CHANGE_PASSWORD);
    }

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ChangePasswordDialog::onButtonBoxClicked);

    QTimer::singleShot(0, this, [this]()
    {
        setFixedHeight(sizeHint().height());
    });
}

//--------------------------------------------------------------------------------------------------
ChangePasswordDialog::~ChangePasswordDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QString ChangePasswordDialog::oldPassword() const
{
    return ui.edit_old_pass->text();
}

//--------------------------------------------------------------------------------------------------
QString ChangePasswordDialog::newPassword() const
{
    return ui.edit_new_pass->text();
}

//--------------------------------------------------------------------------------------------------
void ChangePasswordDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui.button_box->standardButton(button);
    if (standard_button == QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Accepted by user";

        if (mode_ == Mode::CREATE_NEW_PASSWORD)
        {
            QString new_password = ui.edit_new_pass->text();
            QString new_password_repeat = ui.edit_new_pass_repeat->text();

            if (new_password.isEmpty())
            {
                LOG(ERROR) << "Password cannot be empty";
                common::MsgBox::warning(this, tr("Password cannot be empty."));
                ui.edit_new_pass->selectAll();
                ui.edit_new_pass->setFocus();
                return;
            }

            if (new_password != new_password_repeat)
            {
                LOG(ERROR) << "Password entered do not match";
                common::MsgBox::warning(this, tr("The passwords entered do not match."));
                ui.edit_new_pass->selectAll();
                ui.edit_new_pass->setFocus();
                return;
            }
        }
        else
        {
            DCHECK_EQ(mode_, Mode::CHANGE_PASSWORD);

            QString old_password = ui.edit_old_pass->text();
            QString new_password = ui.edit_new_pass->text();
            QString new_password_repeat = ui.edit_new_pass_repeat->text();

            if (old_password.isEmpty())
            {
                LOG(ERROR) << "Old password not entered";
                common::MsgBox::warning(this, tr("You must enter your old password."));
                ui.edit_old_pass->setFocus();
                return;
            }

            if (!SystemSettings::isValidPassword(old_password))
            {
                LOG(ERROR) << "Incorrect password entered";
                common::MsgBox::warning(this, tr("You entered an incorrect old password."));
                ui.edit_old_pass->selectAll();
                ui.edit_old_pass->setFocus();
                return;
            }

            if (new_password.isEmpty())
            {
                LOG(ERROR) << "New password cannot be empty";
                common::MsgBox::warning(this, tr("New password cannot be empty."));
                ui.edit_new_pass->setFocus();
                return;
            }

            if (new_password != new_password_repeat)
            {
                LOG(ERROR) << "Password entered do not match";
                common::MsgBox::warning(this, tr("The passwords entered do not match."));
                ui.edit_new_pass->selectAll();
                ui.edit_new_pass->setFocus();
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

} // namespace host
